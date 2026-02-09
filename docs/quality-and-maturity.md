# Quality and Maturity: Is this project “World Class”?

This document gives an honest assessment of where this project stands today and what would move it toward “world class” for a C++ defect-detection / vision pipeline framework.

---

## What Is Already Strong

These aspects are **at or near the level you’d expect from a serious, production-oriented framework**:

- **Architecture**
  - Clear layering: **core → vision → app** with one-way dependencies; core has no external deps beyond the standard library.
  - Well-defined interfaces: `IPipelineStage`, `IInferenceBackend`, `StageOutput` as variant; pipeline runs a sequence of stages and stops at first `DefectResult`.
  - Good separation: Frame/Defect/DefectResult in core; OpenCV confined to vision; app is thin orchestration.

- **Modern C++**
  - **C++23**: `std::expected` for errors, `std::span` for buffer views, RAII, `enum class`, no raw pointers in the public API.
  - Sensible ownership: `std::unique_ptr<IPipelineStage>`, owned `std::vector<std::byte>` in `Frame`.
  - Thread-safety documented; parallel batch runner with configurable worker count.

- **Build and packaging**
  - **CMake**: target-based, `PUBLIC`/`PRIVATE` deps, install rules, export for `find_package(Normitri)` with `NormitriConfig.cmake` and `find_dependency(OpenCV)`.
  - **Compiler warnings**: `CompilerWarnings.cmake` with `-Wall -Wextra -Wpedantic -Wconversion` (and MSVC equivalents); optional warnings-as-errors.
  - **Conan + Docker**: dependency management and reproducible container build; issue log documents common failures and fixes.

- **Documentation**
  - Architecture, building, dependencies, detection-efficiency, development, API reference, and an issue log (problems and solutions) are all in place.
  - Rationale for using OpenCV and for *not* vendoring it in `third_party/` is documented.

- **Testing**
  - Unit tests for core (frame, defect, pipeline) and vision (decoder, mock backend); integration test for full pipeline and parallel batch.
  - Mock inference backend makes the pipeline testable without a real model.

So: **this project is a solid, professional-quality framework** with a clean design, modern C++, and good documentation. It is suitable for real use and extension.

---

## What “World Class” Usually Adds

“World class” for a C++ framework typically implies **additional maturity and rigor** beyond “well designed and documented.” Below are concrete gaps and improvements that would move this project in that direction.

### 1. Continuous integration (CI)

- **Gap:** No automated build/test on every commit or PR (e.g. GitHub Actions, GitLab CI).
- **Improvement:** Add a CI workflow that:
  - Builds with at least one compiler (e.g. GCC or Clang) on Linux (and optionally Windows/macOS).
  - Runs tests (`ctest` or the test executable).
  - Optionally runs with Conan and with system dependencies to catch both paths.
- **Why it matters:** Prevents regressions and gives contributors confidence; expected in serious open-source and internal frameworks.

### 2. Static analysis and extra tooling

- **Gap:** Only compiler warnings; no clang-tidy, no sanitizers in CI, no code coverage.
- **Improvement:**
  - Add **clang-tidy** (e.g. a subset of checks) and run it in CI or locally via script.
  - Enable **AddressSanitizer** (and optionally UBSan) in a CI or “sanitizer” build to catch memory and undefined behavior bugs.
  - Optionally add **code coverage** (e.g. gcov/lcov) and publish or gate on a minimum coverage (e.g. for core/vision).
- **Why it matters:** Catches bugs and style/ownership issues that compilers miss; coverage highlights untested paths.

### 3. API and ABI stability (if shipped as a library)

- **Gap:** No explicit versioning or ABI policy; breaking changes would be unclear for downstreams.
- **Improvement:**
  - Document **API/ABI policy** (e.g. SemVer: minor = backward-compatible, major = breaking).
  - For installed libraries, consider symbol visibility and stable C++ ABI (avoid exposing fragile inline/ABI-breaking types in public API).
- **Why it matters:** “World class” libraries that others depend on usually state how they evolve.

### 4. Invariants and validation

- **Gap:** Types like `Defect` and `BBox` are simple structs; no enforced invariants (e.g. bbox in [0,1] or non-negative size).
- **Improvement:**
  - Add constructors or factory helpers that validate (e.g. `BBox` in range, `confidence` in [0,1]) and document guarantees.
  - Use `std::expected` or checks in decoder/stages for invalid configs or inputs.
- **Why it matters:** Fewer silent bugs; clearer contracts for users of the API.

### 5. Reproducible dependency builds

- **Gap:** Conan uses floating versions or “latest” compatible; no lockfile or pinned recipe revisions in the repo.
- **Improvement:**
  - Use a **Conan lockfile** (e.g. `conan.lock`) or document exact versions/revisions for “reference” builds.
  - Document how to reproduce the same dependency graph (e.g. for audits or compliance).
- **Why it matters:** Reproducibility is a hallmark of production-grade build systems.

### 6. Benchmarks and performance

- **Gap:** No benchmarks or performance regression tests.
- **Improvement:**
  - Add a small **benchmark** suite (e.g. Google Benchmark) for hot paths: e.g. pipeline run (resize → normalize → mock inference → decode), or batch parallel throughput.
  - Optionally run in CI and track simple metrics (e.g. “pipeline run &lt; X ms per frame”).
- **Why it matters:** Ensures that refactors don’t silently slow down the pipeline; expected in performance-sensitive vision frameworks.

### 7. Optional: plugin / dynamic backends

- **Gap:** Inference backends are linked at build time; no dynamic loading (e.g. ONNX vs TensorRT chosen at runtime).
- **Improvement:** Document a path to **plugin-style backends** (e.g. load a shared library and get an `IInferenceBackend*`) if you need runtime-switchable engines without recompiling.
- **Why it matters:** Not mandatory for “world class,” but many production vision stacks want this flexibility.

---

## Summary

| Aspect              | Current state                          | To approach “world class”                    |
|---------------------|----------------------------------------|---------------------------------------------|
| Architecture        | Strong                                 | —                                           |
| C++ and APIs        | Strong                                 | Add validation/invariants where needed      |
| Build / install     | Strong                                 | Add Conan lockfile / reproducibility       |
| Documentation       | Strong                                 | —                                           |
| Testing             | Good                                   | Add coverage, sanitizer runs                |
| CI                  | Missing                                | Add at least build + test on commit/PR      |
| Static analysis     | Compiler only                          | Add clang-tidy, optional coverage           |
| Performance         | Not measured                           | Add benchmarks, optional CI regression      |
| API/ABI policy      | Undefined                              | Document versioning and compatibility       |

**Bottom line:** You have built a **solid, professional framework** with a clean design and modern C++. It is not yet “world class” in the sense of CI, static analysis, coverage, benchmarks, and explicit API/ABI policy—but the foundation is there. Prioritize **CI (build + test)** first; then add **clang-tidy and sanitizers**, **invariants/validation**, and **benchmarks** as you go. That path will move this project toward the maturity level of widely-used C++ frameworks.
