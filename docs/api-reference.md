# API Reference

This document describes the main types and interfaces of this C++23 project. Implementations will live in the corresponding headers and source files.

---

## Core Module (`normitri::core`)

### Image and frame types

- **`Frame`** — Represents a single image or video frame.
  - Dimensions (width, height), channel count, element type (e.g. `uint8_t`, `float`).
  - Buffer storage (owned or non-owning view); access via `std::span` or raw pointer where needed for interop with CV/ML APIs.

- **`PixelFormat`** — Enum or type describing layout (e.g. RGB, BGR, grayscale, planar vs packed).

### Error handling

- **`PipelineError`** (or equivalent) — Enum or variant of error codes:
  - e.g. `InvalidFrame`, `LoadFailed`, `InferenceFailed`, `InvalidConfig`.
- **`std::expected<T, E>`** — Return type for operations that can fail; `E` is typically `PipelineError` or a small error type.

### Pipeline contract

- **`IPipelineStage`** (or concept `PipelineStage`) — Contract for a single stage:
  - `process(Frame const& in) -> std::expected<Frame, PipelineError>` (or similar).
  - Stages are composable; the pipeline invokes them in sequence.

- **`Pipeline`** — Holds an ordered list of stages; `run(Frame const&) -> std::expected<Result, PipelineError>`.

---

## Vision Module (`normitri::vision`)

### Preprocessing

- **`ResizeStage`** — Resizes input frame to a given width/height (e.g. for inference input size).
- **`NormalizeStage`** — Applies mean/scale or similar normalization (e.g. for neural network input).
- **`ColorConvertStage`** — Converts between pixel formats (e.g. BGR → RGB, grayscale).

Parameters (dimensions, normalization constants) are configurable via constructor or a config struct.

### Inference adapter

- **`IInferenceBackend`** (or concept) — Abstract interface for running inference:
  - `infer(Frame const& input) -> std::expected<InferenceResult, PipelineError>`.
- Concrete backends (ONNX, TensorRT, etc.) implement this interface so the pipeline stays backend-agnostic.

### Results

- **`InferenceResult`** — Holds raw or decoded outputs (e.g. detection boxes, class scores, segmentation mask).
- **`Detection`** — Single detection: bounding box, class id, confidence (used when decoding detection outputs).

---

## App Module

- **`main()`** — Parses CLI (e.g. input path, config path), constructs pipeline from config, runs on one or more frames, and prints or logs results.
- **`PipelineRunner`** (optional) — Wraps pipeline execution with threading (e.g. thread pool or async) for batch or stream processing.

---

## Namespaces Summary

| Namespace | Purpose |
|-----------|---------|
| `normitri` | Top-level; re-exports or aggregates public API. |
| `normitri::core` | Frame, pipeline contract, errors. |
| `normitri::vision` | Preprocessing stages, inference adapter, result types. |
| `normitri::detail` | Implementation details; not part of stable API. |

---

*This API reference will be updated as types and function signatures are added to the codebase. For build and usage, see [Building](building.md) and [Architecture](architecture.md).*
