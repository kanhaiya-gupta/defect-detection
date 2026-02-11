# Preprocessing

This document describes how image preprocessing works in Normitri: what we have today (OpenCV-based stages), how it fits the inference contract, and possible future directions (hardware ISPs, GPU, other libraries).

---

## Overview

Preprocessing turns a raw or decoded **Frame** (from camera, file, or stream) into the format the inference backend expects: fixed dimensions (e.g. 640×640), normalized values (e.g. 0–1), and the right pixel layout (e.g. RGB float, HWC). The pipeline runs preprocessing stages in sequence; the last stage’s output is passed to **DefectDetectionStage**, which calls the backend’s `validate_input()` and `infer()`. So preprocessing must produce a **Frame** that satisfies the [inference input contract](inference-contract.md).

---

## What we have today

### OpenCV-based stages

All current preprocessing uses **OpenCV** in the **vision** layer:

| Stage | Purpose | OpenCV usage |
|-------|---------|---------------|
| **ResizeStage** | Scale to model input size (e.g. 640×640) | `cv::resize()` (after converting Frame → `cv::Mat`) |
| **NormalizeStage** | Map pixel values (e.g. 0–255 → 0–1) | `cv::Mat::convertTo()` with scale and mean |
| **ColorConvertStage** | Change format (e.g. BGR→RGB) | `cv::cvtColor()` |

- **Frame ↔ OpenCV**: Helpers `frame_to_mat()` and `mat_to_frame()` (in `frame_cv_utils`) convert between our **Frame** (width, height, format, buffer) and **cv::Mat**. Stages use these so that OpenCV never appears in the core pipeline API; only the vision layer depends on OpenCV.
- **Configuration**: Resize dimensions and normalize mean/scale come from **PipelineConfig** (e.g. `resize_width`, `resize_height`, `normalize_mean`, `normalize_scale`) and are passed into the stage constructors when the pipeline is built (e.g. in the CLI or app). The inference contract (dimensions, value range, layout) is documented in [inference-contract.md](inference-contract.md).

### Pipeline order

Typical order is: **Resize → Normalize → ColorConvert**. That way we resize once to the target size, then normalize and convert color on the smaller buffer. The output format is **Float32Planar**, HWC, with dimensions and value range matching the backend’s expectations.

### Why OpenCV

OpenCV is mature, portable, and well aligned with common inference stacks. For resize, normalize, and color conversion in C++, it is a strong default choice. See [Dependencies](dependencies.md) for why we use OpenCV and why we do not vendor it.

---

## Future and alternatives

Preprocessing could be extended or complemented in the future without replacing the current design.

### Hardware ISPs (Image Signal Processors)

Many **cameras** and **SoCs** (e.g. embedded, edge devices) have dedicated **image signal processors** that can do resize, color correction, and sometimes simple normalization in **hardware** before the CPU (or GPU) ever sees the image. In those setups:

- You would typically use the **camera or SoC vendor API** to configure the ISP (resolution, color space, crop, etc.) and receive already-resized or already-converted buffers.
- That does **not** replace OpenCV in Normitri in general: you might feed the pipeline with frames that are “pre-preprocessed” by the ISP (e.g. already at 640×640 or already in a given color space), and then use our **ResizeStage** / **NormalizeStage** / **ColorConvertStage** only when needed (e.g. to match the exact model input or value range). Or you could, in principle, implement a stage that wraps an ISP output path and produces a **Frame**; the rest of the pipeline stays the same.
- So: **Hardware ISPs are a complementary option** for deployment (offload resize/color to hardware where available); the pipeline and OpenCV-based stages remain the place where we define “what the model sees.”

### GPU preprocessing

For very high throughput, some systems do resize/normalize/color conversion on **GPU** (e.g. OpenCV’s `cv::cuda`, or CUDA/OpenCL kernels). That would mean adding optional GPU-capable stages or backends that produce a **Frame** in the same format as today; the inference contract would not change. OpenCV would still be used for CPU preprocessing and for many deployments.

### Other CPU libraries (Halide, libvips, etc.)

Libraries like **Halide** or **libvips** can sometimes give higher throughput for specific pipelines (e.g. hand-tuned resize or convolution). If we ever needed that, we could add a stage that uses such a library and outputs a **Frame**; the pipeline interface and inference contract stay the same. OpenCV would remain the default for portability and simplicity.

### Summary

| Topic | Today | Future / note |
|-------|--------|----------------|
| **Resize, normalize, color** | OpenCV in vision layer (ResizeStage, NormalizeStage, ColorConvertStage) | Keep as default; optional GPU or ISP paths can feed or complement. |
| **Hardware ISPs** | Not used; pipeline assumes Frame from app/camera. | Use vendor API for ISP output; feed resulting Frame into pipeline; use OpenCV stages only when still needed. |
| **Contract** | [Inference contract](inference-contract.md) (dimensions, format, range) | Unchanged; any future preprocessing must still produce a Frame that satisfies it. |

---

## Related docs

- [Inference contract](inference-contract.md) — Format and layout the backend expects; preprocessing is configured to match.
- [Dependencies](dependencies.md) — Why we use OpenCV and where it is used.
- [Architecture](architecture.md) — Pipeline stages and where preprocessing sits (vision layer).
- [Workflow](workflow.md) — Diagrams of the pipeline including preprocessing steps.
