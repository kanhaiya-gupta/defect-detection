# Development Guide

## Coding Standards

- **C++23** — Prefer standard library and language features from C++20/23 (e.g. `std::expected`, concepts, ranges) over custom or legacy patterns.
- **Naming** — `snake_case` for functions and variables; `PascalCase` for types. Namespaces in `snake_case` or single-word (e.g. `normitri::core`).
- **Headers** — Use include guards or `#pragma once`. Keep public interfaces minimal; implementation details in `.cpp` or in a `detail/` namespace.
- **Error handling** — Use `std::expected<T, E>` for recoverable failures in the pipeline; reserve exceptions for truly exceptional cases. Define a small set of error types (e.g. `enum class PipelineError` or a variant).
- **Memory** — Prefer RAII and smart pointers; avoid raw `new`/`delete`. Use `std::span` and non-owning views where zero-copy is desired.

## Threading and memory management

- **Memory** — Frame owns a single buffer (`std::vector<std::byte>`); Pipeline and stages use `std::unique_ptr` for ownership. Move semantics everywhere; no raw `new`/`delete`. Use `std::span` for non-owning views (e.g. `Frame::data()`).
- **Threading** — `Pipeline::run()` is safe to call from multiple threads concurrently (stages are not modified during `process()`). `run_pipeline_batch_parallel()` uses a thread pool (mutex + condition_variable + queue of indices); the callback can be invoked from any worker and must be thread-safe (e.g. lock before pushing to a shared container).
- **Ownership** — Libraries own their stages and backends via `unique_ptr`; callers pass `const Frame&` or `vector<Frame>` by const ref to avoid copies when running the pipeline.

## Repository Layout

See [Architecture – Detailed Directory and File Layout](architecture.md#detailed-directory-and-file-layout) for the full tree. Summary:

```
Normitri/
├── CMakeLists.txt
├── conanfile.txt / conanfile.py    # optional
├── README.md, LICENSE
├── .github/workflows/
├── cmake/                  # Find*.cmake, CompilerWarnings, sanitizers, etc.
├── include/normitri/       # Public API headers only (core/, vision/, app/)
├── src/                     # Implementation (.cpp) and private headers (core/, vision/, app/)
├── apps/                    # Executables (e.g. normitri-cli/main.cpp, later normitri-service)
├── tests/
│   ├── unit/                # per-module
│   ├── component/           # multi-module
│   └── integration/         # full pipeline
├── examples/                # optional demos
├── third_party/             # optional – only if vendoring
└── docs/
```

## Module Contents (Defect Detection for Shopping Items)

### `include/normitri/core/` and `src/core/`

- **Public headers** (in `include/normitri/core/`): **Frame**, **PixelFormat**, **PipelineError**, **Defect**, **DefectResult**, **IPipelineStage**, **Pipeline**.
- **Implementation** (in `src/core/`): `.cpp` for Frame, Pipeline, and any private/detail code.
- **Defect types** — e.g. `WrongItem`, `WrongQuantity`, `ExpiredOrQuality`, `ProcessError`, with optional bbox, confidence, product_id, category.

### `include/normitri/vision/` and `src/vision/`

- **Public headers**: preprocessing stages, **IInferenceBackend**, **InferenceResult**, **DefectDecoder**, **DefectDetectionStage**, **MockInferenceBackend**.
- **Implementation**: stage `.cpp`, decoder `.cpp`, mock backend `.cpp`.
- **Defect-detection stage** composes backend + decoder and produces **DefectResult**.

### `src/app/` and `apps/`

- **Shared app logic** (in `src/app/`): **PipelineRunner** (thread pool/async), **Config** loading (stages, model path, per-category/per-defect-type thresholds), **image_source** abstraction.
- **Executables** (in `apps/`): e.g. **normitri-cli** — `main.cpp` parses CLI (input path, config), builds pipeline, runs, outputs defects. Add **normitri-service** or other apps as separate targets under `apps/` without changing the layout.

## Testing

- **Framework** — Google Test or Catch2, integrated via CMake.
- **Scope** — Unit tests for core types and each pipeline stage in isolation; integration tests for a short pipeline (e.g. load image → preprocess → mock inference).
- **Running** — `ctest --test-dir build` or run the test executable directly. Aim to run tests in CI on every push.

## Static Analysis and Formatting

- **Clang-Format** — Use a project `.clang-format` for consistent style.
- **Clang-Tidy** — Enable in CI or locally with a `clang-tidy` config; focus on memory, concurrency, and modern C++ checks.
- **Warnings** — Build with `-Wall -Wextra` (GCC/Clang) or `/W4` (MSVC); treat warnings as errors in CI (`-Werror` / `/WX`).

## CI/CD

Suggested CI (e.g. GitHub Actions):

- **Build** — Ubuntu (GCC, Clang) and Windows (MSVC); CMake configure and build in Release and Debug.
- **Tests** — Run CTest after build; optional coverage upload.
- **Lint** — Clang-format check, optional clang-tidy run.

Optional: Docker image for a reproducible build environment; separate workflow or job for building the Docker image.

## Adding a New Pipeline Stage

1. Define the stage type in the appropriate namespace (e.g. `normitri::vision`).
2. Implement the pipeline stage interface (e.g. `process(Frame const&) -> std::expected<Frame, Error>`).
3. Add unit tests with synthetic or small fixture images.
4. Register the stage in the pipeline configuration (e.g. factory or config file) so the app can use it.

## Debugging and Performance

- **Debug** — Use `CMAKE_BUILD_TYPE=Debug` and run under a debugger; ensure frame data and pipeline state are inspectable.
- **Performance** — Profile with perf (Linux), VTune, or similar; optimize hot paths (e.g. resize, normalize) and consider SIMD or GPU backends for heavy stages. Keep the pipeline interface unchanged so backends can be swapped.
