# Implementation Plan: Real Defect Detection

This document describes the plan to replace the **mock inference backend** with a **real inference backend** so that the pipeline reports defects based on actual image content instead of a fixed synthetic result.

---

## Current state

- The pipeline runs **preprocessing** (resize, normalize, color convert) on real image data.
- **Inference** is done by **MockInferenceBackend**, which ignores the frame and always returns the same hardcoded defect (e.g. one WrongItem, confidence 0.95, fixed bbox).
- **DefectDecoder** and the rest of the pipeline work correctly; only the inference step is fake.
- Result: every image (defective or good) gets the same “1 defect” output.

**Goal:** Use a real model (e.g. ONNX, TensorRT) so that defect presence, kind, confidence, and bbox reflect the actual image.

---

## Architecture (already in place)

The codebase is already structured for pluggable inference:

| Component | Role |
|-----------|------|
| **IInferenceBackend** | Interface: `infer(Frame) -> expected<InferenceResult, PipelineError>`. Optional: `validate_input()`, `infer_batch()`, `warmup()`. |
| **InferenceResult** | Framework-agnostic: `boxes`, `scores`, `class_ids`, `num_detections`. Backends convert native output (ONNX/TensorRT tensors) into this. |
| **DefectDecoder** | Converts InferenceResult → `vector<Defect>`: confidence threshold, NMS, class ID → DefectKind mapping. |
| **DefectDetectionStage** | Holds `unique_ptr<IInferenceBackend>` + DefectDecoder; calls `validate_input()` then `infer()` then `decode()`. |

Input contract (format the backend receives): [inference-contract.md](inference-contract.md).  
Design and gaps: [inference-design-and-gaps.md](inference-design-and-gaps.md).

---

## Implementation plan

### Phase 1: ONNX Runtime backend (CPU first)

1. **Add dependency**  
   - Conan: add **onnxruntime** (or opencv’s DNN module with ONNX) to `conanfile.txt`, or use a system/vcpkg package. Prefer **ONNX Runtime** for a clear, maintained C++ API.

2. **Implement ONNX backend class**  
   - New type implementing **IInferenceBackend** (e.g. `include/normitri/vision/onnx_inference_backend.hpp`, `src/vision/onnx_inference_backend.cpp`).  
   - Constructor/factory: load ONNX model from path (and optional session options).  
   - **validate_input()**: check frame dimensions and format match model input (e.g. 640×640, Float32Planar or as needed).  
   - **infer()**: copy or map Frame buffer into model input tensor(s), run session, read output tensors (boxes, scores, class_ids), fill **InferenceResult**.  
   - Handle layout (HWC vs NCHW) and value range per [inference-contract.md](inference-contract.md) and model expectations.  
   - **warmup()**: run one dummy inference after construction.

3. **Model and class mapping**  
   - Choose or train a **detection model** (e.g. YOLO-style, SSD) that outputs boxes + scores + class IDs.  
   - Define **class ID → DefectKind** mapping (e.g. in config or in DefectDecoder’s `ClassToDefectKindMap`).  
   - Document expected class IDs and optional NMS params in config.

4. **Config and CLI**  
   - Extend **PipelineConfig** (or equivalent) with: model path, optional backend type (`mock` vs `onnx`), device/thread options.  
   - In **normitri-cli** (or app that builds the pipeline): if config says “onnx”, construct **OnnxInferenceBackend** and pass to DefectDetectionStage; else keep MockInferenceBackend.  
   - Ensure **DefectDecoder** is built with the same class→DefectKind map the model uses.

   **How to choose mock vs ONNX (and later TensorRT):**
   - **Config file**: set `backend_type=mock` or `backend_type=onnx`, and `model_path=/path/to/model.onnx` when using ONNX. Pass the file with `--config <path>`.
   - **CLI overrides**: `--backend mock` | `--backend onnx` and `--model <path>` override (or supply) backend and model without a config file.
   - **Default**: if no config and no `--backend`, **mock** is used. For ONNX you must set the backend (via config or `--backend onnx`) and provide a model path (config or `--model`).
   - When TensorRT is implemented, add `backend_type=tensorrt` / `--backend tensorrt` and a TensorRT engine path.

   **Models: where to put ONNX files, and how many**
   - **Where to put them:** There is no fixed location. The path is whatever you pass in **config** (`model_path=...`) or on the **CLI** (`--model <path>`). It can be absolute (e.g. `/opt/models/detector.onnx`) or relative to the process current working directory (e.g. `models/detector.onnx`). A common choice is a **`models/`** directory in the repo or in your deployment tree (e.g. `models/detector.onnx`), and then run the CLI from the repo root or set `model_path=models/detector.onnx` in your config. Do not commit large binary models to git if you use repo `models/`; use `.gitignore` or a separate artifact store and copy files into `models/` at deploy time.
   - **One model per pipeline:** Each pipeline has **one** inference backend, and each backend is constructed with **one** model path. So at any moment, **one pipeline uses one ONNX file**. You can have **100 or more ONNX files** on disk (e.g. one per customer, per product category, or per camera type). In that case you build 100 pipelines (or 100 backend instances), each with a different `model_path` pointing to one of those files. The application layer decides which pipeline (and thus which model) to use for each frame (e.g. by customer_id or camera_id). So: one model per pipeline instance; many files on disk and many pipelines are supported.

5. **Testing**  
   - Unit tests: mock Frame → ONNX backend → check InferenceResult shape and sanity (e.g. num_detections, score range).  
   - Integration: run pipeline with a small ONNX model (or a minimal “identity”-style model) and assert DefectResult reflects model output.  
   - Keep **MockInferenceBackend** for CI and tests that don’t have a real model.

### Phase 2: Production hardening (optional, after Phase 1)

- **GPU**: ONNX Runtime with CUDA execution provider; device selection in backend config.  
- **TensorRT backend**: second implementation of **IInferenceBackend** using TensorRT; same InferenceResult contract.  
- **infer_batch()**: override in ONNX/TensorRT backend for batched input when using `run_pipeline_batch_parallel`.  
- **Model lifecycle**: load in factory; call **warmup()** once before first real frame (already in contract).

**Should you implement TensorRT?** Yes, if you target GPU and want the best inference latency. TensorRT is optional: you can start with **ONNX Runtime + CUDA** (same ONNX model, GPU-accelerated) and add a **TensorRT backend** later when you need maximum throughput or want to use pre-built TensorRT engines (`.engine`). The pipeline stays the same; you add `backend_type=tensorrt` and an engine path.

**Building for GPU (e.g. Lightning AI):** The code can be built and run on GPU hosts (Lightning AI, AWS, on-prem with NVIDIA GPU). Two options:

1. **ONNX Runtime + CUDA** — Add the CUDA execution provider to the existing ONNX backend; build with Conan/CMake on a machine that has CUDA and cuDNN. No TensorRT required; one build gives you GPU inference with your `.onnx` model.
2. **TensorRT** — Add a TensorRT backend (new class implementing `IInferenceBackend`), Conan dependency on `tensorrt` (and CUDA/cuDNN), and build on a GPU host. You then export your model to a TensorRT engine (`.engine`) or build the engine at runtime from `.onnx`. Use `--backend tensorrt --model /path/to/model.engine` (or `.onnx` if you build the engine in the backend).

Both options produce a single binary that runs on the GPU machine; the application layer chooses the backend via config or CLI. For Lightning AI: use a GPU-enabled image (e.g. CUDA base), install Conan dependencies with TensorRT/CUDA options as needed, and build Normitri; the resulting binary will run inference on the GPU.

### Deployment: many customers / cameras

When many customers are scanning or many cameras are feeding frames, structure parallelism at **customer level** or **camera level**, not only at frame level:

- **One pipeline (and one inference backend) per customer or per camera.** Route each incoming frame to the pipeline for that customer/camera; run each pipeline from a single thread (or a small dedicated pool). That avoids shared-backend thread-safety issues, gives isolation and predictable latency per stream, and scales by adding more pipelines.
- The **application layer** (above Normitri) is responsible for routing frames by `customer_id`/`camera_id`, creating and owning one pipeline per unit, and tagging results in callbacks. Normitri provides the single-pipeline and batch APIs; it does not implement “pipeline per customer/camera” itself.

**Is it easy to do with our framework?** Yes. You do not need to change Normitri. Build one pipeline per customer/camera using the same logic you use today (e.g. the same `build_pipeline(cfg)` or equivalent): each pipeline is an independent object that owns its own stages and inference backend. Keep a map (e.g. `camera_id → Pipeline` or `customer_id → Pipeline`). When a frame arrives for camera K, look up the pipeline for K and call `run_pipeline(pipeline_for_K, frame)` — from a thread dedicated to that camera, or from a pool that executes “process this frame for this camera” tasks. Each pipeline is used from at most one thread at a time, so the backend stays single-threaded and you avoid thread-safety issues. The only code you add is application-level: routing (which pipeline for this frame?), pipeline creation and storage (e.g. at startup or on first frame per camera), and optional threading (one thread per camera, or a worker pool that takes (camera_id, frame) and calls `run_pipeline` with the right pipeline). No new Normitri APIs are required.

See [Threading and memory management](threading-and-memory-management.md#customer-level-or-camera-level-parallelism-recommended-for-production) for the full recommendation and comparison with frame-level-only parallelism.

### Phase 3: TBB for multi-camera / multi-user parallelism (optional)

**Goal:** Use Intel TBB to schedule **per-camera or per-customer** work so that many cameras (or users) can be processed in parallel on a GPU server, with good load balance and no shared inference backend.

**Why TBB:** The current `run_pipeline_batch_parallel()` uses a std::thread pool and a single shared pipeline; that forces either a thread-safe backend or a single worker. The recommended production model is **one pipeline (and one backend) per camera or per customer**. TBB adds task-based scheduling and work stealing: you submit one task per “(camera_id, frame)” and TBB runs them across workers. Each task uses only that camera’s pipeline, so each ONNX/TensorRT session is single-threaded and GPU use scales with the number of cameras.

**Scope:**

1. **Optional TBB dependency** — CMake option (e.g. `NORMITRI_USE_TBB`, default OFF or ON with auto-detect). Find TBB via `find_package(TBB)` or Conan. When TBB is not found, keep using the existing std::thread-based `run_pipeline_batch_parallel()`.
2. **Multi-camera / multi-tenant runner** — New API that accepts a **map of unit id → pipeline** (e.g. `camera_id → Pipeline` or `customer_id → Pipeline`) and a **batch of (unit_id, frame)** work items. Implementation uses a TBB task arena (or `parallel_for` over work items): each task runs `pipeline_for_unit.run(frame)` and invokes the callback with the result and unit id. No shared backend; each pipeline is used from at most one task at a time (per unit, if you serialize per unit) or from one worker at a time (if work items are independent and pipelines are not shared across units).
3. **Concrete API shape (to be refined)** — For example:
   - `run_pipeline_multi_camera_tbb(pipelines_by_camera, frames_by_camera, callback)` where `pipelines_by_camera` is `std::unordered_map<std::string, std::reference_wrapper<Pipeline>>` and `frames_by_camera` is `std::unordered_map<std::string, std::vector<Frame>>` (or a single flat list of `(camera_id, frame)` pairs). Callback receives `(DefectResult, camera_id)`. TBB schedules one task per (camera_id, frame); each task uses only `pipelines_by_camera[camera_id]`.
   - Alternatively, a **runner class** that holds the map of pipelines and exposes `submit(camera_id, frame)` and `run()` / drain, using a TBB flow graph or task group under the hood.
4. **Backend and GPU** — Each pipeline continues to own one inference backend (ONNX or TensorRT). On a **GPU server**, build with TensorRT (or ONNX + CUDA) and create one pipeline per camera; each pipeline loads its own engine/session. GPU memory and throughput scale with the number of cameras; TBB balances CPU work (preprocessing, post-processing) and schedules which (camera, frame) runs next.

**Order vs GPU:** You can do Phase 3 (TBB) first and then run on a GPU server, or run on the GPU server first with the current std::thread batch API (e.g. one worker per camera or one pipeline per camera and manual threading). Implementing TBB gives a clean, scalable way to drive many pipelines (many cameras/users) on one machine; the GPU server then runs that same binary with TensorRT backends.

**Files to add or change (Phase 3):**

| Area | Action |
|------|--------|
| **CMakeLists.txt** | Option `NORMITRI_USE_TBB`; `find_package(TBB)` or Conan; when found, build TBB-based runner and define `NORMITRI_HAS_TBB`. |
| **conanfile.txt** | Optional: add `tbb` when using Conan for TBB. |
| **include/normitri/app/** | New header for multi-camera/multi-tenant runner (e.g. `pipeline_runner_tbb.hpp` or extend `pipeline_runner.hpp` with `#ifdef NORMITRI_HAS_TBB`). |
| **src/app/** | New or conditional implementation using TBB task arena / `parallel_for` over (unit_id, frame) work items. |
| **docs/** | Update [Threading and memory management](threading-and-memory-management.md) to describe TBB-based multi-camera API and when to use it. |

---

## Files to add or change

| Area | Action |
|------|--------|
| **conanfile.txt** | Add `onnxruntime` (or chosen package) when introducing ONNX backend. |
| **include/normitri/vision/** | Add `onnx_inference_backend.hpp` (class implementing IInferenceBackend). |
| **src/vision/** | Add `onnx_inference_backend.cpp` (load model, run session, fill InferenceResult). |
| **include/normitri/app/config.hpp** | Extend config with model path, backend type, optional device/threads. |
| **src/app/config.cpp** | Parse new options from config file or defaults. |
| **apps/normitri-cli/main.cpp** | Build pipeline with OnnxInferenceBackend when config specifies real backend; otherwise keep mock. |
| **CMakeLists.txt** | Link ONNX backend and normitri_cli to ONNX Runtime (or chosen lib). |
| **tests/** | Add unit test for ONNX backend (with a small test model or fixture); integration test that runs pipeline with real backend if model present. |
| **docs/** | Update README / building.md with “how to run with real model” and where to put the ONNX file (e.g. `models/` or config path). |

| **Phase 2 — TensorRT** | **conanfile.txt**: add `tensorrt` (and CUDA/cuDNN as required). **include/** and **src/vision/**: add `tensorrt_inference_backend.hpp` and `.cpp` implementing `IInferenceBackend`. **config**: add `InferenceBackendType::TensorRT`; reuse `model_path` for engine. **main.cpp**: construct TensorRT backend when `backend_type==TensorRT`; CLI `--backend tensorrt`. **CMakeLists.txt**: link TensorRT libs (auto-detect when GPU available). **docs/building.md**: document GPU build. |
| **Phase 3 — TBB** | **CMakeLists.txt**: option `NORMITRI_USE_TBB`, find TBB, build TBB-based multi-camera runner when available. **include/normitri/app/** and **src/app/**: new or conditional API for per-camera/per-customer parallelism using TBB task arena. **docs/threading-and-memory-management.md**: document TBB API and usage on GPU server. |

---

## Success criteria

- With **mock** backend: behavior unchanged (current tests and CLI).  
- With **ONNX** backend and a real detection model:  
  - Different images (e.g. good vs defective) produce different DefectResults (count, kind, or confidence).  
  - Output is written to terminal and to `output/<basename>.txt` as today.  
- CI continues to pass using MockInferenceBackend; optional job or script to run with ONNX + test model if available.
- **Phase 3 (TBB):** When TBB is enabled, multi-camera runner schedules (camera_id, frame) work with one pipeline per camera; no shared backend; runs on GPU server with TensorRT backends per pipeline.

---

## References

- [Inference contract](inference-contract.md) — Frame format, validation, lifecycle.  
- [Inference design & gaps](inference-design-and-gaps.md) — Why the interface is backend-agnostic and what’s missing for production.  
- [Threading and memory management](threading-and-memory-management.md) — Parallelism, backend thread safety, customer/camera-level scaling.  
- [Architecture](architecture.md) — Pipeline stages and DefectDetectionStage.  
- [Building](building.md) — Conan, CMake, and how to add dependencies.
