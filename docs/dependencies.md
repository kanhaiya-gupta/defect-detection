# Dependencies: OpenCV and Why We Do Not Vendor It

This document explains why this project uses **OpenCV** and why we **do not** ship it (or GTest) from a `third_party/` folder.

---

## Why We Use OpenCV

We use **OpenCV** in the **vision** layer for image preprocessing that feeds the defect-detection pipeline.

### Technical reasons

- **Resize, normalize, color conversion** — The pipeline needs reliable, well-tested image operations: resizing to model input size (e.g. 640×640), normalization (e.g. to [0,1] or model-specific ranges), and color conversion (e.g. BGR→RGB, YUV→RGB). OpenCV provides these with consistent APIs, good performance (including optional GPU/accelerated paths), and wide platform support.
- **Mature and stable** — OpenCV is the de facto standard for computer vision in C++. Using it reduces risk and maintenance compared to reimplementing or maintaining our own image-processing stack.
- **Alignment with typical inference stacks** — Many inference runtimes (ONNX Runtime, TensorRT) and training frameworks expect images in formats and layouts that OpenCV handles well. Using OpenCV keeps our preprocessing consistent with common ML pipelines and makes it easier to plug in different backends later.
- **Portability** — OpenCV is available on Linux, Windows, macOS, and embedded targets (e.g. Jetson, ARM). We can rely on system packages or Conan for consistent dependency resolution across environments.

### Where we use it

OpenCV is used only in the **vision** layer (e.g. `ResizeStage`, `NormalizeStage`, `ColorConvertStage`) and in helpers that convert between our `Frame` type and `cv::Mat`. The **core** layer stays dependency-free (standard library only), so the choice of OpenCV does not leak into core types or the pipeline abstraction.

---

## Why We Do Not Use OpenCV from a `third_party/` Folder

We **do not** vendor or bundle OpenCV (or GTest) in a **`third_party/`** directory. Dependencies are provided by:

- **Conan** (recommended): `conan install` fetches and builds OpenCV (and GTest) into the build directory, or
- **System packages**: e.g. `libopencv-dev`, or another system-level package manager (vcpkg, etc.).

### Reasons we do not vendor OpenCV

1. **Size and build cost** — OpenCV is large (many modules, optional dependencies, and build options). Checking it into the repo or vendoring it via a submodule would bloat the repository and slow clone/CI. Building it from source inside the project would complicate our CMake and increase build times for every developer and CI run.
2. **Security and updates** — OpenCV receives security and bug fixes over time. Relying on Conan or the system package manager lets us get updated versions (and CVEs addressed) by updating the Conan recipe version or the system package, rather than maintaining a vendored copy and backporting patches.
3. **Consistency and reproducibility** — Conan and system packages give a single, well-defined way to get a compatible OpenCV build (version, options, ABI). Vendoring would require us to document and maintain our own build configuration and to keep it in sync with our supported platforms.
4. **Ecosystem alignment** — Many teams already use Conan or system OpenCV for other projects. Using the same mechanism keeps the project consistent with common C++/vision workflows and makes it easier to integrate into existing build and deployment pipelines.
5. **Clarity of scope** — The `third_party/` folder in our layout is reserved for rare cases where we truly need to vendor a small, stable dependency. OpenCV and GTest do not fit that profile; documenting that we rely on external dependency management avoids confusion and sets clear expectations for contributors.

### When `third_party/` might be used

We use **`third_party/`** only when we intentionally vendor a dependency (e.g. a small, stable, header-only or single-file library that we need to pin or patch). OpenCV and GTest are **not** in that category; they remain external dependencies provided by Conan or the system.

---

## Summary

| Topic | Decision |
|-------|----------|
| **Use OpenCV?** | Yes — for vision-layer preprocessing (resize, normalize, color conversion); mature, portable, and aligned with common inference stacks. |
| **Where is OpenCV used?** | Only in the **vision** layer and Frame↔cv::Mat helpers; **core** has no dependency on OpenCV. |
| **Ship OpenCV in repo?** | No — we do not put OpenCV (or GTest) in `third_party/`. |
| **How to get OpenCV?** | Via **Conan** (recommended) or **system** packages; see [Building](building.md#dependencies-opencv-and-gtest-required). |

For build instructions and options (Conan vs system, Docker), see [Building the Project](building.md).
