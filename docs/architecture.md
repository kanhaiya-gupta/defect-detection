# Architecture

## Overview

The project is structured as a **modular image analytics pipeline** for **defect detection in shopping items** (wrong item, wrong quantity, quality/expiry, process errors). It is suitable for edge deployment: configurable stages (preprocessing, inference, post-processing) with clear interfaces and minimal coupling.

For a **visual workflow overview** (Mermaid diagrams: single-frame, end-to-end, batch/parallel), see [Pipeline workflow](workflow.md).

## High-Level Design

```
┌─────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Image      │────▶│  Pipeline        │────▶│  DefectResult      │
│  Source     │     │  (stages)        │     │  / Alerts           │
└─────────────┘     └──────────────────┘     └─────────────────┘
                            │
                            ▼
                    ┌──────────────────┐
                    │  Inference       │  (plug-in: ONNX, TensorRT, Mock)
                    │  Backend         │
                    └──────────────────┘
```

---

## Detailed Architecture

### Layered View

Dependencies flow **one way**: **app → vision → core**. Core has no dependency on vision or app; vision depends only on core.

```
┌─────────────────────────────────────────────────────────────────┐
│  APPLICATION LAYER (app)                                         │
│  main(), CLI, PipelineRunner, config loader                       │
│  Depends on: vision, core                                         │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  VISION LAYER (vision)                                            │
│  Preprocessing stages, DefectDetectionStage, inference adapter,  │
│  decoder, mock backend                                            │
│  Depends on: core only                                            │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  CORE LAYER (core)                                                │
│  Frame, PixelFormat, PipelineError, Defect, Pipeline, IPipelineStage
│  Depends on: std library only                                     │
└─────────────────────────────────────────────────────────────────┘
```

### Detailed Directory and File Layout

```
Normitri/
├── CMakeLists.txt
├── conanfile.txt                   # optional (OpenCV, GTest, etc.)
├── conanfile.py                    # optional – more powerful Conan config
├── README.md
├── LICENSE
├── .github/workflows/              # CI (e.g. build, test, lint)
├── cmake/                          # Helper modules, Find*.cmake, warnings, sanitizers
│   ├── NormitriConfig.cmake
│   ├── CompilerWarnings.cmake
│   └── ...
│
├── include/normitri/               # Public headers only (installable API)
│   ├── core/
│   │   ├── frame.hpp               # Frame, PixelFormat, buffer access (span)
│   │   ├── error.hpp               # PipelineError enum
│   │   ├── defect.hpp              # Defect: kind, bbox, confidence, product_id, optional category/product_group
│   │   ├── pipeline_stage.hpp      # IPipelineStage concept/interface
│   │   ├── pipeline.hpp
│   │   └── defect_result.hpp       # DefectResult: frame_id, list<Defect>, optional metadata
│   ├── vision/
│   │   ├── resize_stage.hpp
│   │   ├── normalize_stage.hpp
│   │   ├── color_convert_stage.hpp
│   │   ├── inference_backend.hpp   # IInferenceBackend interface
│   │   ├── inference_result.hpp
│   │   ├── defect_decoder.hpp      # configurable thresholds (per type/category)
│   │   ├── defect_detection_stage.hpp
│   │   └── mock_inference_backend.hpp
│   └── app/                        # optional – if you expose app helpers as API
│       └── ...
│
├── src/                            # Private headers + implementation (.cpp)
│   ├── core/                       # frame.cpp, pipeline.cpp, any private/detail
│   ├── vision/                     # stage .cpp, decoder .cpp, mock backend .cpp
│   └── app/                        # pipeline_runner, config, image_source (shared app logic)
│
├── apps/                           # Executables (one or more)
│   ├── normitri-cli/               # CLI: input path, config, run pipeline, output defects
│   │   └── main.cpp
│   └── (e.g. normitri-service/      # optional – future daemon/service)
│
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/                       # Per-module unit tests (frame_test, pipeline_test, decoder_test, …)
│   ├── component/                  # Multi-module component tests
│   └── integration/                # Full-pipeline / end-to-end (e.g. image → DefectResult)
│
├── examples/                       # Small self-contained demos (optional)
├── third_party/                    # Optional – only if vendoring a dependency
├── docs/
└── (build out-of-tree, e.g. build/)
```

- **Include path**: Consumers and tests use `#include <normitri/core/frame.hpp>` (or similar) with `include/` or the install prefix on the include path.
- **Apps**: Each app (e.g. `normitri-cli`) is a separate CMake target linking the normitri libraries; add `normitri-service` or others later without changing this layout.
- **Tests**: **unit/** per-module; **component/** for multi-module; **integration/** for full pipeline with mock or real backend.
- **cmake/**: Reusable scripts for compiler warnings, sanitizers, and `find_package(Normitri)` for downstream projects.
- **third_party/**: Only if you vendor a dependency. We do not vendor OpenCV or GTest; see [Building](building.md#dependencies-opencv-and-gtest-required) and [Dependencies](dependencies.md) for rationale.

### Detection efficiency and configuration

Design choices that affect the layout above are documented in [Detection efficiency](detection-efficiency.md). In short:

- **Defect** carries optional **category** / **product_group** (for reporting and prioritisation); **no price/value** in core (value comes from product master downstream if needed).
- **Config** supports **per-category** or **per-defect-type** confidence thresholds and optional **high-value categories** for alerting; decoder and app use these so precision/recall are tunable without code changes.
- Detection is **category-agnostic** by default; category-specific behaviour is configuration-driven (e.g. which model, which thresholds), not hard-coded.

### Defect-Detection Pipeline Flow (Detailed)

```
  [Image Source]
        │
        ▼
  ┌─────────────┐
  │ Frame       │  (raw or decoded image: width, height, format, buffer)
  └─────────────┘
        │
        ▼
  ┌─────────────────────────────────────────────────────────────────┐
  │ PREPROCESSING (vision stages)                                     │
  │   ResizeStage → NormalizeStage → ColorConvertStage                │
  │   In: Frame  →  Out: Frame (e.g. 640×640, float, RGB)             │
  └─────────────────────────────────────────────────────────────────┘
        │
        ▼
  ┌─────────────────────────────────────────────────────────────────┐
  │ DEFECT DETECTION STAGE (vision)                                   │
  │   1. Receives preprocessed Frame                                  │
  │   2. IInferenceBackend::infer(frame) -> InferenceResult            │
  │   3. DefectDecoder::decode(InferenceResult) -> vector<Defect>      │
  │   4. DefectResult { frame_id, defects }                            │
  └─────────────────────────────────────────────────────────────────┘
        │
        ▼
  ┌─────────────┐
  │ DefectResult│  { frame_id, defects: [ WrongItem, WrongQuantity, ... ] }
  └─────────────┘
        │
        ▼
  [App: log, alert, send to POS, or display]
```

### Component Responsibilities (Detailed)

| Component | Layer | Responsibility |
|-----------|--------|----------------|
| **Frame** | core | Image dimensions, pixel format, buffer (owned or view); `data()` as span; copy/move safe. |
| **PipelineError** | core | Enum: InvalidFrame, LoadFailed, InferenceFailed, InvalidConfig, DecoderError. |
| **Defect** | core | Kind (WrongItem, WrongQuantity, ExpiredOrQuality, ProcessError), bbox, confidence, product_id, optional category/product_group. |
| **DefectResult** | core | frame id(s), vector of Defect, optional metadata. |
| **IPipelineStage** | core | Concept/interface: process(Frame) -> expected&lt;output, PipelineError&gt;. |
| **Pipeline** | core | Ordered list of stages; run(Frame) executes and collects DefectResult. |
| **ResizeStage, NormalizeStage, ColorConvertStage** | vision | Implement IPipelineStage; Frame -> Frame for inference input. |
| **IInferenceBackend** | vision | infer(Frame) -> expected&lt;InferenceResult, PipelineError&gt;. |
| **InferenceResult** | vision | Raw model outputs (detection tensor, scores, boxes). |
| **DefectDecoder** | vision | InferenceResult -> vector&lt;Defect&gt;; configurable confidence thresholds, NMS; optional per-defect-type or per-category thresholds. |
| **DefectDetectionStage** | vision | Composes backend + decoder; produces DefectResult. |
| **MockInferenceBackend** | vision | Returns synthetic defects for tests/demo. |
| **apps/** (e.g. normitri-cli), **Config, PipelineRunner** | app | Executables in apps/ (CLI, config path, run pipeline, output defects); src/app/: config parsing, pipeline runner, image source. |

### Dependency Rules

- **core**: No project dependencies beyond the C++ standard library.
- **vision**: Depends on **core** only. No dependency on **app**.
- **app**: Depends on **vision** and **core**.
- **tests**: Link core, vision, optionally app; use MockInferenceBackend for integration tests.

---

## Modules

| Module | Responsibility |
|--------|----------------|
| **core** | Frame, PixelFormat, PipelineError, Defect, DefectResult, IPipelineStage, Pipeline. |
| **vision** | Preprocessing stages, IInferenceBackend, InferenceResult, DefectDecoder, DefectDetectionStage, MockInferenceBackend. |
| **app** | Executables in apps/ (e.g. normitri-cli); shared logic in src/app/: Config, PipelineRunner, image source adapter. |

## Pipeline Stages

Stages implement a common interface (e.g. `Process(Frame const&) -> std::expected<Frame, Error>`). Examples:

- **Preprocess** — Resize, normalize, format conversion (e.g. for a specific inference input layout)
- **Inference** — Abstract interface; concrete backends (ONNX, TensorRT) can be added without changing the pipeline
- **Postprocess** — Decode detections, filter, aggregate

New stages are added by implementing the stage contract and registering them in the pipeline configuration.

## Data Flow

- **Frame** — Represents a single image (or video frame): dimensions, format, and raw buffer. Designed for zero-copy views where possible (C++23 spans / non-owning views).
- **DefectResult** — Final pipeline output: frame id(s) and list of Defects (wrong item, wrong quantity, etc.) for alerts, logging, or POS integration.
- **InferenceResult** — Backend-specific raw output (tensors, boxes, scores) produced by the inference stage before decoding.

## Threading and Concurrency

- Pipeline execution can run in a thread pool or async tasks.
- Frame buffers are either owned per stage or passed via shared ownership to avoid unnecessary copies across stages.
- Synchronization is explicit at pipeline boundaries; stages can be marked as thread-safe or single-threaded in configuration.

## C++23 Usage

- **`std::expected`** — Used for operations that can fail (load image, run stage) instead of exceptions where appropriate.
- **Concepts** — Used to constrain pipeline stages and image types.
- **Ranges / views** — Used for iteration over frames or detection results where it improves clarity.
- **`if consteval` / `std::is_constant_evaluated`** — Where compile-time vs runtime paths differ (e.g. tests vs production).

## Extension Points

- **Inference backend** — Implement the inference adapter interface and plug in ONNX Runtime, TensorRT, or other engines. For how well the current design supports production, multiple frameworks, and hardware, see [Inference design and gaps](inference-design-and-gaps.md).
- **Image source/sink** — Cameras, video files, HTTP/MQTT streams can be added behind a common source/sink interface.
- **New stages** — Any new processing step (e.g. tracking, counting) is added as a new stage type and registered in the pipeline.
