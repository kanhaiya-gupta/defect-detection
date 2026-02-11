# Threading and Memory Management

This document describes how Normitri manages memory and concurrency: ownership, RAII, thread safety of the pipeline and batch runner, and what backends must guarantee when used in parallel. It also explains the current parallel processing model (no TBB) and options for scaling to many customers or cameras.

---

## Memory management

### Ownership and RAII

- **Frame** owns a single buffer: `std::vector<std::byte>`. Move semantics and RAII; no shared ownership. Use `data()` for non-owning `std::span` views. See [core/frame.hpp](../include/normitri/core/frame.hpp).

- **Pipeline** owns its stages: `std::vector<std::unique_ptr<IPipelineStage>>`. Stages are not copied or shared.

- **DefectDetectionStage** owns the inference backend: `std::unique_ptr<IInferenceBackend>`. The backend is created once (e.g. in the CLI or app) and moved into the stage.

- **OnnxInferenceBackend** uses the pimpl idiom: `std::unique_ptr<Impl>` so ONNX Runtime headers stay out of the public API. `Impl` holds `Ort::Env`, `Ort::SessionOptions`, and `Ort::Session` — all are RAII types in the ONNX Runtime C++ API and are destroyed when `Impl` is destroyed (reverse order of declaration).

- **No raw `new` / `delete`**: Allocations use `std::make_unique`, `std::vector`, or stack. No manual ownership.

### Pipeline data flow

- **Pipeline::run()** keeps a `std::variant<Frame, DefectResult>` as the current value. It starts with a copy of the input `Frame`; each stage returns by value (either a new `Frame` or a `DefectResult`). The variant is updated by move; returned `DefectResult` is moved or copied out to the caller. No buffers are shared across stages.

- **DefectResult** and **InferenceResult** are value types: `std::vector<Defect>`, `std::vector<float>`, etc. Ownership is clear; no raw pointers to heap.

### ONNX backend details

- **Input tensor**: For NCHW, data is copied into `impl_->nchw_buffer` (a `std::vector<float>`). `Ort::Value::CreateTensor` is given a pointer to that buffer; the buffer outlives `session.Run()`. For NHWC, the pointer passed to `CreateTensor` is the `Frame`’s buffer; the `Frame` is alive for the duration of `infer()` (caller’s frame). ONNX Runtime uses the pointer only during `Run()` and does not take ownership.

- **Output tensors**: `session.Run()` returns `std::vector<Ort::Value>`. Each `Ort::Value` is RAII and releases the underlying tensor when it goes out of scope. We read from them (e.g. `GetTensorData<float>()`) and copy into `InferenceResult` before the function returns, so no dangling pointers.

- **Output name pointers**: `impl_->output_name_ptrs` holds `const char*` pointing at `impl_->output_names[i].c_str()`. Both live in the same `Impl`; the pointers are used only while `Impl` exists and are not stored elsewhere. Safe.

### Frame ↔ OpenCV views

- **frame_to_mat()** (used in resize, normalize, color-convert stages) builds a `cv::Mat` that **views** the `Frame`’s buffer (no copy). The `Mat` must not be used after the `Frame` is destroyed. In the current code, `frame_to_mat(input)` is used only within a stage’s `process()`; the `Frame` is the input and stays alive for the whole call. The stage then creates a **new** `Frame` (e.g. via `mat_to_frame()` with a new buffer) and returns it. So view lifetime is correct.

- **mat_to_frame()** allocates a new `std::vector<std::byte>`, copies from the `cv::Mat`, and returns a `Frame` that owns that buffer. No shared ownership.

### Summary

Memory is managed with standard C++ ownership: RAII, `unique_ptr`, and value types. No raw `new`/`delete`; no ownership ambiguity. The only non-owning pointers are into buffers that outlive their use (e.g. `output_name_ptrs` into `output_names`, or a `Mat` viewing a `Frame` for the duration of one `process()` call).

---

## Threading

### Pipeline::run()

- **Thread-safe for concurrent calls**: Multiple threads may call `Pipeline::run()` at the same time with different `Frame` inputs. The pipeline does not modify its stages during `run()`; each call uses the same stages but with different data. So concurrent `run()` is safe as long as stages and backends do not rely on mutable shared state without synchronization.

- **Single-threaded per call**: Within one `run()`, stages are executed sequentially on the calling thread. There is no internal parallelism inside a single `run()`.

### run_pipeline_batch_parallel()

- **Model**: The main thread fills a queue with frame indices. A pool of worker threads repeatedly take an index, call `pipeline.run(frames[idx])`, and pass the result to a callback. The **same** `Pipeline` (and thus the same `DefectDetectionStage` and inference backend) is used by all workers.

- **Implication**: The inference backend’s `infer()` (and optionally `infer_batch()`) may be **invoked from multiple threads concurrently**. So any backend used with `run_pipeline_batch_parallel` must either be thread-safe or the application must avoid using batch parallel with that backend.

- **Frames**: Workers only read `frames[idx]`; the vector of frames is not modified. The callback receives the result by value (`*result`), so the application can process or store it without sharing mutable state with the pipeline.

### Backend thread safety

- **Contract** (see [Inference contract](inference-contract.md)): Backends must document whether `infer()` and `infer_batch()` are thread-safe. The pipeline may call them from multiple threads when using `run_pipeline_batch_parallel`.

- **MockInferenceBackend**: Uses internal state (`defects_to_return_`). It is **not** thread-safe for concurrent `infer()` or `infer_batch()` unless that state is never changed after construction or is protected externally.

- **OnnxInferenceBackend**: Uses a single `Ort::Session`. ONNX Runtime’s `Session::Run()` is generally **not** thread-safe for the same session. So concurrent `infer()` on the same backend is unsafe. For parallel batch processing with ONNX, either: use one backend (and session) per thread, or add an internal mutex in the ONNX backend around `infer()` (and document it). The current implementation does not add a mutex; safe for single-threaded use or one pipeline per thread.

### Recommendations

- **Single-threaded or one run at a time**: Use the pipeline as today; no change.
- **run_pipeline_batch_parallel with ONNX**: Either ensure only one thread runs inference (e.g. single worker), or make the ONNX backend thread-safe (e.g. mutex around `infer()`), or build one pipeline (and backend) per thread so each session is used by a single thread.

---

## Parallel processing: many customers / cameras

### What we use today (no TBB)

Normitri does **not** use Intel TBB (Threading Building Blocks) or OpenMP. Parallelism for defect detection is done with the standard library only:

- **`run_pipeline_batch_parallel()`** — A fixed-size **thread pool** (default size = `std::thread::hardware_concurrency()`). A shared **queue** holds frame indices. Each worker thread repeatedly:
  1. Takes an index from the queue (under a mutex).
  2. Calls `pipeline.run(frames[idx])` (full pipeline: resize → normalize → … → inference → decode).
  3. Invokes the callback with the `DefectResult` (callback must be thread-safe).

So **many frames** (e.g. from many customers scanning at once) can be processed **in parallel**: each frame is handled by one call to `Pipeline::run()` on a worker thread. Throughput is limited by the number of workers and by the inference backend (see below).

- **Single frame**: Use `run_pipeline()` — no threads; one frame in, one result out.

### Customer-level or camera-level parallelism (recommended for production)

**Yes — for many customers or many cameras you should structure parallelism at customer or camera level, not only at frame level.**

Today’s **`run_pipeline_batch_parallel()`** is **frame-level**: one shared queue of frames, N workers, one shared pipeline. There is no notion of “customer A” or “camera 2”. That is fine for a single batch of anonymous frames, but for production with many customers scanning or many cameras you get better behaviour with **customer-level** or **camera-level** parallelism:

| Aspect | Frame-level only (current API) | Customer / camera-level |
|--------|--------------------------------|---------------------------|
| **Isolation** | One heavy stream can delay others; no per-customer/camera fairness. | Each customer or camera has dedicated capacity; one bad stream doesn’t starve the rest. |
| **Backend thread safety** | All workers share one pipeline/backend → need thread-safe backend or single worker. | One pipeline (and one backend instance) per customer/camera → each backend is single-threaded, no shared session. |
| **Latency / SLA** | Latency for a given frame depends on total load. | You can bound latency per customer or per camera (e.g. one thread per camera). |
| **Resource control** | Single pool; hard to give “customer X” a guaranteed share. | Easy to assign resources per customer/camera (e.g. one pipeline per camera, or N workers per customer). |

**Recommended model for many customers / cameras:**

- **Per camera**: One pipeline (and one inference backend instance) per camera. Each camera has its own thread or small pool that runs `run_pipeline()` or `run_pipeline_batch_parallel(..., num_workers=1)` on that camera’s frames. No shared backend; no thread-safety concern. You can run many such “camera pipelines” in parallel (e.g. one thread per camera, or a pool that executes “run pipeline for camera K” tasks).
- **Per customer**: Same idea: one pipeline (and backend) per customer or per checkout lane. Route each incoming frame to the pipeline for that customer/camera (e.g. by `customer_id` or `camera_id`). Each unit is single-threaded or has its own small pool; scaling is “more customers/cameras → more pipelines,” not “more workers on one shared backend.”

The **application layer** (above Normitri) is responsible for:
- **Routing**: When a frame arrives (e.g. from a camera or a scan), assign it a `customer_id` or `camera_id`, and enqueue it to the corresponding pipeline or queue for that unit.
- **Creating pipelines**: At startup or on demand, create one `Pipeline` (and one `IInferenceBackend`) per customer or per camera. Each pipeline is then used from one thread (or a dedicated small pool) so the backend never sees concurrent calls.
- **Callbacks**: Each unit’s callback can tag results with the same `customer_id`/`camera_id` for downstream (POS, alerts, dashboards). **DefectResult** has optional **`camera_id`** and **`customer_id`** (strings) so every result carries which camera and which customer it relates to. Use `run_pipeline(..., camera_id, customer_id)` for single-frame, or pass `camera_ids` / `customer_ids` vectors to `run_pipeline_batch` and `run_pipeline_batch_parallel` so each result is tagged before the callback.

Normitri does **not** implement this routing or “pipeline per customer/camera” for you; it provides the single-pipeline and batch-parallel APIs. Building **customer-level or camera-level parallelism** means using those APIs in a **per-customer or per-camera** way (one pipeline per unit, one or more threads per unit). That is the right design for “many customers scanning” or “many cameras.”

### Fit for “many customers scanning” (frame-level API)

- **Batch of frames**: If you have a batch of frames (e.g. from several cameras or a queue of scans), you can call `run_pipeline_batch_parallel(pipeline, frames, callback, num_workers)`. Frames are processed concurrently across workers. The callback is invoked from worker threads whenever a result is ready; it must be thread-safe. For production with many customers/cameras, prefer **one pipeline per customer/camera** and feed each pipeline only that unit’s frames (see above).
- **Inference bottleneck**: If you keep a **single** shared pipeline, all workers share the **same** inference backend. If the backend is not thread-safe (e.g. current ONNX backend with one session), you must use `num_workers == 1` or make the backend thread-safe (e.g. mutex), or switch to **one pipeline (and backend) per customer/camera** so each session is single-threaded.
- **No TBB**: The current design avoids a TBB dependency: it uses `std::thread`, `std::mutex`, and `std::condition_variable`. That is sufficient for a fixed pool of workers and a single shared queue. TBB would add task-based parallelism, work stealing, and potentially better load balance; it is optional for a future iteration if you need it.

### Optional: TBB or other libraries later

If you need to scale further (e.g. many more concurrent streams, better load balance, or integration with other TBB-based code):

- **TBB**: You could introduce a dependency on Intel TBB and use it to run **per-customer or per-camera** work: e.g. a task per camera that runs that camera’s pipeline on its frames. That fits the recommended “one pipeline per camera” model; TBB would schedule those tasks and balance load.
- **Dedicated inference process**: For very high throughput, inference is sometimes offloaded to a separate process or service (e.g. a GPU server) that receives frames and returns results; the “many customers” side then only enqueues work and collects results.

Summary: **Yes, we use parallel processing for defect detection** — via `run_pipeline_batch_parallel()` and a std::thread-based pool. For **many customers or many cameras**, you should add **customer-level or camera-level** parallelism: one pipeline (and backend) per customer or per camera, with routing and callbacks in the application layer. We do **not** use TBB today; it can be added later for task-based scheduling of those per-customer/camera pipelines if needed.

---

## Related docs

- [Inference contract](inference-contract.md) — Lifecycle, warmup, and backend thread-safety contract.
- [Inference design & gaps](inference-design-and-gaps.md) — Backend interface and production considerations.
- [Architecture](architecture.md) — Pipeline and stage design.
