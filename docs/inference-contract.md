# Inference Input Contract

Backends implementing `IInferenceBackend` receive a **Frame** from the pipeline. This document defines the **expected format and layout** so preprocessing stages can be configured correctly and backends can validate input.

---

## Default contract (single-frame inference)

- **Pixel format:** `PixelFormat::Float32Planar` (float per channel). Typical preprocessing: `NormalizeStage` produces float; `ColorConvertStage` can produce RGB float.
- **Layout:** Contiguous buffer in **HWC** (height, width, channels) order — same as OpenCV’s default `cv::Mat` after `convertTo(CV_32FC3)`. Backends that need **NCHW** (e.g. some ONNX/TensorRT models) must transpose or accept HWC and document it.
- **Dimensions:** Backend-specific. Configure `ResizeStage` to the model’s expected input size (e.g. 640×640). Backends may override `validate_input()` to check width/height.
- **Value range:** Typically **[0, 1]** after normalization. Configure `NormalizeStage(0.f, 1.f/255.f)` for 0–255 input, or model-specific mean/scale.
- **Channels:** Usually 3 (RGB). Match `ColorConvertStage` output to the model (e.g. RGB vs BGR).

Backends that need a different contract (e.g. NCHW, different range) must document it and implement `validate_input()` to reject invalid frames.

---

## Validation

- **`IInferenceBackend::validate_input(const Frame&)`** — Optional override. Default implementation returns success. Backends can check dimensions, format, and buffer size and return `PipelineError::InvalidFrame` if the frame is not suitable.
- Call `validate_input()` before `infer()` (e.g. in `DefectDetectionStage`) to fail fast with a clear error.

---

## Batch inference

When using **`infer_batch()`**, each frame in the batch must satisfy the same contract as for single-frame inference. Batch size and any backend-specific limits are documented by the backend.

---

## Lifecycle and warmup

- **Model load:** Done in the backend constructor or factory; not part of the pipeline interface.
- **Warmup:** Call **`warmup()`** once after constructing the backend (e.g. before processing the first real frame) to run a dummy inference. Useful for TensorRT and other runtimes that optimize on first run.
- **Thread safety:** Backends must document whether `infer()` / `infer_batch()` are thread-safe. The pipeline may call them from multiple threads when using `run_pipeline_batch_parallel`.
