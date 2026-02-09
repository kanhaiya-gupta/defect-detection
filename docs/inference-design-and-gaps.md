# ML/AI Inference: What We Did Right and What’s Missing for Production

This document assesses how well this project handles ML/AI inference relative to a **Senior C++/Computer Vision Engineer** role: deploying to production, scaling across customers, working with **different ML/AI inference frameworks**, and **optimizing for different hardware architectures**.

---

## What We Did Right

### 1. **Backend-agnostic abstraction**

- **`IInferenceBackend`** has a single responsibility: `infer(Frame) -> expected<InferenceResult, PipelineError>`.
- The pipeline never depends on ONNX, TensorRT, or any specific runtime. Concrete backends (ONNX Runtime, TensorRT, Mock, or future engines) **implement this interface** and are plugged in via `DefectDetectionStage(std::unique_ptr<IInferenceBackend>)`.
- This matches the role’s requirement to “get to work with different ML/AI inference frameworks”: the **architecture is ready**; we only add new backend implementations.

### 2. **Clear separation: inference vs decoding**

- **InferenceResult** is a simple, framework-agnostic structure: `boxes`, `scores`, `class_ids`, `num_detections`. Each backend adapts its native output (ONNX tensors, TensorRT bindings, etc.) into this format.
- **DefectDecoder** turns `InferenceResult` into `vector<Defect>` (with threshold, NMS, class→DefectKind mapping). So model-specific decoding lives in the backend or decoder; the rest of the app stays uniform.
- That keeps “different ML frameworks” behind one contract and lets you optimize or swap backends without rewriting the pipeline.

### 3. **Pipeline as the integration point**

- Preprocessing (resize, normalize, color convert) produces a **Frame** that the backend consumes. The pipeline is the single place where “input image → inference input” is defined, which fits production and customer-specific pipelines (e.g. different input sizes per model).
- **DefectDetectionStage** composes backend + decoder and produces **DefectResult**; the rest of the system (alerts, POS, dashboards) depends only on DefectResult, not on the inference engine.

### 4. **Testability without a real model**

- **MockInferenceBackend** returns configurable synthetic results. Unit and integration tests run the full pipeline (preprocess → “inference” → decode) without ONNX/TensorRT or GPU. That’s essential for CI and for “managing customer-specific customizations” without needing every model in CI.

### 5. **Thread-level parallelism for throughput**

- **`run_pipeline_batch_parallel`** runs multiple frames in parallel (thread pool). Each thread calls `Pipeline::run()` with its own frame; backends that are thread-safe can scale with core count. That addresses “scaling product deployments” at the process level.

---

## Gaps for Production, Multiple Frameworks, and Hardware

These are the areas that a Senior C++/CV Engineer would expect to tackle next.

### 1. **No real inference backend yet**

- Only **MockInferenceBackend** is implemented. The interface is designed for ONNX/TensorRT, but we haven’t proven it with a real runtime (memory layout, error handling, GPU buffers).
- **Recommendation:** Implement at least one **ONNX Runtime** backend (CPU first, then GPU if needed). That validates the contract (Frame → InferenceResult), clarifies lifecycle (load/warmup/unload), and gives a reference for TensorRT or other engines.

### 2. **InferenceResult is detection-specific**

- Today: flat `boxes`, `scores`, `class_ids`, `num_detections`. That fits many detection models (YOLO, SSD, etc.) but not every output shape (e.g. raw logits, segmentation masks, multi-head outputs).
- **Recommendation:** Keep the current shape as the **primary** contract. For models that don’t match, either:
  - Adapt inside the backend (e.g. ONNX backend reshapes tensors into boxes/scores/class_ids), or
  - Introduce an optional “raw” or extensible slot (e.g. `std::optional<std::vector<float>> extra_tensors`) and document when to use it. Avoid turning InferenceResult into a generic “any tensor” type; keep the common path simple.

### 3. **Frame → backend contract** ✓ Addressed

- **Done:** [Inference input contract](inference-contract.md) documents expected format (Float32Planar, HWC, dimensions, value range). **`validate_input(const Frame&)`** added to `IInferenceBackend` (default: accept); backends can override to reject invalid frames. **DefectDetectionStage** calls `validate_input()` before `infer()` for fail-fast.

### 4. **Batch inference in the interface** ✓ Addressed

- **Done:** **`infer_batch(span<const Frame>)`** added to `IInferenceBackend`. Default implementation loops over `infer()`; backends (e.g. ONNX/TensorRT) can override for true batched GPU inference. **MockInferenceBackend** overrides and returns one result per frame.

### 5. **No hardware/device abstraction**

- There is no notion of “run on GPU 0”, “CPU only”, or “TensorRT on GPU, ONNX on CPU”. Backend construction (e.g. from config or factory) would today be the place to pass device id or backend type.
- **Recommendation:** Keep device selection **inside** each backend (e.g. ONNX Runtime backend gets `provider: "CUDAExecutionProvider"` and device id in its config). Document that backends are responsible for device placement and that the pipeline only sees Frame (CPU buffer). For GPU-backed Frame in the future, you’d extend Frame or add a “GPU buffer” type and let backends accept that; that’s a larger change and can follow once a real GPU backend exists.

### 6. **Model lifecycle** ✓ Partially addressed

- **Done:** **`warmup()`** added to `IInferenceBackend` (default: no-op). Call once after construction for runtimes that optimize on first run (e.g. TensorRT). [Inference contract](inference-contract.md) documents lifecycle (load in constructor/factory; call warmup before first real frame). Model loading remains outside the interface (factory/builder).

### 7. **Latency / SLO instrumentation** ✓ Addressed

- **Done:** **Optional per-stage timing:** `Pipeline::run(input, &timing_cb)` and `run_pipeline(pipeline, frame, &timing_cb)` accept a **StageTimingCallback** `(stage_index, duration_ms)`. Callback is invoked after each stage. Supports SLO tuning and “inference &lt;10 ms” checks (inference is typically one stage).

### 8. **Single backend per pipeline**

- Today one pipeline has one `DefectDetectionStage` with one backend. For “different recognition paths” (e.g. produce vs barcode) or per-customer models, you’d build different pipelines or swap backends at config time.
- **Recommendation:** No change required for the first version. Multiple backends (e.g. one per defect type or per product category) can be modeled as multiple pipelines or multiple stages that each call a different backend. Revisit only if you need “one pipeline, multiple models” with a single API.

---

## Summary Table

| Topic | Current state | Recommendation for production / role |
|-------|----------------|--------------------------------------|
| **Multiple frameworks** | Interface is backend-agnostic; only Mock implemented | Implement ≥1 real backend (e.g. ONNX Runtime); keep InferenceResult as common contract |
| **Different hardware** | No device abstraction in API | Keep device choice inside backend config; document; add GPU/device when adding GPU backend |
| **Scalability** | ✓ **infer_batch()** added; thread-parallel batch still supported | Real backends can override infer_batch for GPU batching |
| **Frame contract** | ✓ **Documented** ([inference-contract.md](inference-contract.md)); **validate_input()** in interface and DefectDetectionStage | Backends override validate_input() as needed |
| **Model lifecycle** | ✓ **warmup()** in interface; contract doc describes lifecycle | Load in factory; call warmup() before first frame |
| **Latency / SLO** | ✓ **StageTimingCallback** in Pipeline::run and run_pipeline | Use callback to log/alert on stage duration (e.g. inference &lt;10 ms) |
| **InferenceResult** | Detection-oriented (boxes, scores, class_ids) | Keep; adapt other model types in backend or extend with optional extra tensors |

---

## Bottom Line for the Role

- **Did we handle ML/AI inference correctly?** **Yes, at the design level:** the pipeline is backend-agnostic, the interface is clear, and the separation between inference and decoding supports multiple frameworks and customer-specific models. You can truthfully say we designed for “different ML/AI inference frameworks” and “optimizing for different hardware” (with device choice inside backends).
- **What’s missing for “production” and “scale”?** A **real backend** (ONNX or TensorRT), a **documented Frame contract**, optional **batch inference** and **latency instrumentation**, and a clear **model lifecycle** story. Addressing these is the natural next step for a Senior C++/CV Engineer joining the project.

Use this doc to explain in interviews or with stakeholders: “We got the architecture right; the next phase is implementing and tuning real backends and hardening for production and hardware diversity.”
