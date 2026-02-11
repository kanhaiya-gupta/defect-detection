# Building the Project

## Prerequisites

- **Compiler** — GCC 13+, Clang 16+, or MSVC 2022 17.6+ with C++23 support
- **CMake** — 3.21 or newer
- **OpenCV** — **Required** (vision stages: resize, normalize, color conversion).
- **GTest** — **Required** when building tests (`NORMITRI_BUILD_TESTS=ON`, default). Provide via Conan or system packages.
- **Optional** — Conan 2.x to supply both OpenCV and GTest; otherwise install both system-wide. Or use **Docker** (no Conan/OpenCV install on host).

## When do you need Conan?

You **don’t** need Conan on your machine if you use **Docker** — the image runs Conan inside the container.

You **do** need Conan if you build on the host and want the project script to fetch dependencies (OpenCV, GTest, ONNX Runtime, TBB). In that case install it once:

```bash
pip install conan
# or: pip install --user conan   then ensure `conan` is on PATH
```

Check with `conan --version` (Conan 2.x). Then run `./scripts/build_nomitri.sh install` and `./scripts/build_nomitri.sh build` as below. Alternatively, install OpenCV and GTest (and optionally onnxruntime, TBB) via your system package manager and configure CMake without the Conan toolchain (see [Dependencies](dependencies.md)).

## Dependencies: OpenCV and GTest Required

**OpenCV** and **GTest** are required dependencies. Vision uses OpenCV for image ops; tests use GTest. When `NORMITRI_BUILD_TESTS=OFF`, only OpenCV is required.

| Method | How |
|--------|-----|
| **Conan** | `conan install . --output-folder=build --build=missing` then configure with `-DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake` (provides OpenCV and GTest). |
| **System** | Install OpenCV and GTest (e.g. `apt install libopencv-dev libgtest-dev`, or vcpkg), then `cmake -B build`; CMake runs `find_package(OpenCV REQUIRED)` and `find_package(GTest REQUIRED)`. |

We do **not** vendor OpenCV or GTest in a **`third_party/`** folder; they are always provided by Conan or the system. For the rationale (why we use OpenCV and why we do not put it in `third_party/`), see [Dependencies: OpenCV and why we do not vendor it](dependencies.md).

## Getting Started

### One script: install + build (recommended)

From the repo root (Linux, macOS, WSL, or Git Bash on Windows):

```bash
chmod +x scripts/*.sh   # once, if you get "permission denied"
./scripts/build_nomitri.sh install   # Install deps via Conan (uses --build=missing)
./scripts/build_nomitri.sh build     # Configure and build (uses Conan toolchain if present)
# or
./scripts/build_nomitri.sh all       # install then build
```

The script uses **`--build=missing`** so Conan builds any package that has no pre-built binary for your platform (e.g. onnxruntime, date, onetbb). That keeps local and CI builds working. The script writes the toolchain to `build/conan_toolchain.cmake`; `build` then uses it automatically.

**Building locally (error-free before Lightning AI):** Use the script above. If you run Conan manually, always pass **`--build=missing`**:  
`conan install . --output-folder=build --build=missing`  
Otherwise you may see "No compatible configuration found" for some packages when no pre-built binary exists for your compiler/OS.

**Troubleshooting Conan**

| Error | Fix |
|-------|-----|
| **default profile doesn't exist** / **You need to create a default profile** | Run once: **`conan profile detect`**. Then run `./scripts/build_nomitri.sh install` again. |
| **No compatible configuration found** (date, onnxruntime, onetbb, etc.) | Use **`--build=missing`** so Conan builds from source. Run `./scripts/build_nomitri.sh install` (it already passes `--build=missing`) or `conan install . --output-folder=build --build=missing`. |
| **onetbb/... Invalid: requires hwloc/*:shared=True** | The conanfile already sets `hwloc/*:shared=True` in `[options]`. Ensure you use the project’s conanfile.txt and run with `--build=missing`. |

### Build with Docker (no Conan/OpenCV on host)

Use **`scripts/docker_nomitri.sh`** so you don’t have to remember Docker options. From repo root (WSL, Linux, or macOS):

```bash
./scripts/docker_nomitri.sh              # default: install deps + build (builds image if needed)
./scripts/docker_nomitri.sh all         # same
./scripts/docker_nomitri.sh install     # Conan install only
./scripts/docker_nomitri.sh build       # CMake build only
./scripts/docker_nomitri.sh test        # run tests (ctest)
./scripts/docker_nomitri.sh run --help  # run normitri_cli inside container
./scripts/docker_nomitri.sh shell       # interactive bash in container
./scripts/docker_nomitri.sh image       # (re)build Docker image only
```

Your `build/` directory is on the host (volume mount), so after `./scripts/docker_nomitri.sh` you can run `./build/apps/normitri-cli/normitri_cli` locally. On Windows use WSL or Git Bash to run the script; for PowerShell you can run the Docker commands manually (see script for the exact `docker run` invocation).

**If you see `$'\r': command not found` or `invalid option`** when running the script in WSL/Linux, the file has Windows line endings. Run once: `sed -i 's/\r$//' scripts/*.sh`. The repo’s `.gitattributes` forces `*.sh` to use LF on checkout so this should not recur after a fresh clone.

### Configure and build (out-of-tree, manual)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run the application

```bash
./build/apps/normitri-cli/normitri_cli   # or build\apps\normitri-cli\Release\normitri_cli.exe on Windows
```

### Run tests

```bash
ctest --test-dir build
# or
./build/tests/normitri_tests
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | — | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `NORMITRI_USE_CONAN` | `OFF` | Use Conan to provide dependencies (e.g. OpenCV, GTest) |
| `NORMITRI_BUILD_TESTS` | `ON` | Build the unit test target |
| `NORMITRI_BUILD_APP` | `ON` | Build the main application |
| `NORMITRI_USE_TENSORRT` | `ON` | If ON, look for TensorRT/CUDA and build the TensorRT backend when found; if OFF, skip. Set OFF in CI without GPU. |
| `NORMITRI_CXX_STANDARD` | `23` | C++ standard (20 or 23 recommended) |

Example with options:

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DNORMITRI_USE_CONAN=ON \
  -DNORMITRI_BUILD_TESTS=ON
cmake --build build
```

## Using Conan

If Conan is installed and `NORMITRI_USE_CONAN=ON`:

1. Install dependencies (from project root):

   ```bash
   conan install . --output-folder=build --build=missing
   ```

2. Configure CMake with the Conan toolchain:

   ```bash
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

A `conanfile.txt` or `conanfile.py` in the project root defines required packages (e.g. `opencv`, `gtest`).

## CI and CD (GitHub Actions)

The repo uses five **trigger-based** workflows: **CI** (build + test with GCC and Clang), **Docker** (build and test inside the project image), **Format** (clang-format check), **CodeQL** (security/code analysis), and **Release** (CD: create GitHub Release and upload artifacts). None run on push or PR by default—run from the **Actions** tab when needed. See [.github/workflows/README.md](../.github/workflows/README.md) for a short overview. CI builds with `NORMITRI_WARNINGS_AS_ERRORS=ON`; to reproduce locally: `NORMITRI_CMAKE_ARGS=-DNORMITRI_WARNINGS_AS_ERRORS=ON ./scripts/build_nomitri.sh build`.

**Releases:** Create and push a version tag (e.g. `git tag v0.1.0 && git push origin v0.1.0`), then in GitHub go to **Actions → Release → Run workflow**, select the tag ref, enter the version (e.g. `v0.1.0`), and run. The workflow builds in Docker, runs tests, creates the GitHub Release, and uploads the Linux x64 CLI binary, source zip, and SHA256SUMS.

## IDE and Editors

- **Visual Studio** — Open the folder and use “CMake” project; ensure “C++23” is set in CMake settings.
- **VS Code** — Use CMake Tools extension; configure kit and variant (Debug/Release) in the status bar.
- **CLion** — Open the project root; CMake is detected automatically. Set C++ standard to 23 in CMakeLists.txt or in CMake options.

## Building with TensorRT (optional, GPU)

The **TensorRT** inference backend is **enabled automatically** when CMake finds TensorRT and CUDA on the build machine (e.g. on Lightning AI or any GPU host with TensorRT installed). No extra CMake option is required.

1. **Install TensorRT and CUDA** on the build machine (e.g. from [NVIDIA TensorRT](https://developer.nvidia.com/tensorrt) and [CUDA Toolkit](https://developer.nvidia.com/cuda-downloads), or via package manager: `apt install libnvinfer-dev libcudart-dev` on Debian/Ubuntu when using the NVIDIA repo).
2. **Configure and build as usual** (e.g. `./scripts/build_nomitri.sh all` or `cmake -B build && cmake --build build`). If TensorRT/CUDA are found, you will see “TensorRT/CUDA found: building TensorRT inference backend” and the CLI will support `--backend tensorrt`.
3. If TensorRT is not found, the build continues without the TensorRT backend (ONNX and mock still work). To disable auto-detection (e.g. in CI without GPU), pass `-DNORMITRI_USE_TENSORRT=OFF`.
4. **Run** with an engine file: `./build/apps/normitri-cli/normitri_cli --backend tensorrt --model /path/to/model.engine --input data/images/sample_shelf.jpg`

The engine file (`.engine`) must be **built from an ONNX model on the target GPU** (do not download a pre-built `.engine`; it is GPU- and TensorRT-version-specific). After [downloading an ONNX model](../models/README.md) (e.g. `./scripts/download_onnx_models.sh`), run **`./scripts/build_tensorrt_engine.sh`** on a machine with TensorRT and a GPU to produce a `.engine`; or use `trtexec --onnx=... --saveEngine=...` directly. Input/output layout must match what the pipeline expects (see [Inference contract](inference-contract.md)).

**TensorRT unit tests:** When TensorRT is built, `normitri_vision_tests` includes TensorRT backend tests. One test (constructor with missing file) always runs; the rest require a real engine. Set `NORMITRI_TEST_TENSORRT_ENGINE` to the path of a `.engine` file (e.g. 640×640 input) to run them; if unset, those tests are skipped so CI without a GPU/engine still passes.

## Running on Lightning AI

[Lightning AI](https://lightning.ai) provides cloud **Studios** (IDE + terminal + optional GPU). You can build and run Normitri there for development or GPU inference without managing a local GPU.

### 1. Create or open a Studio

1. Go to [Lightning AI](https://lightning.ai) and sign in.
2. Open your project or create one, then go to **Studios**.
3. **Create** a new Studio or **Open** an existing one.
4. When prompted, choose a **machine type**:
   - **CPU** — Enough for building and running with the **mock** or **ONNX** backend (CPU inference).
   - **GPU** (e.g. T4, A10G) — For the **TensorRT** backend or ONNX with CUDA; select a GPU instance so TensorRT/CUDA are available in the image.
5. Start the Studio and wait until the **terminal** (and optionally file browser) is ready.

### 2. Get the code

In the Studio terminal, clone the repo (or upload your fork):

```bash
git clone https://github.com/kanhaiya-gupta/defect-detection.git
cd defect-detection
```

If the repo is private, use a personal access token or SSH key configured in the Studio.

### 3. Build

Install Conan only if it’s not already available (`conan --version`). If you get **permission denied** when running the script, make the scripts executable once: `chmod +x scripts/*.sh`. Then run the build:

```bash
conan --version || pip install conan   # install Conan only if missing
conan profile detect                    # create default Conan profile (once; fixes "default profile doesn't exist")
chmod +x scripts/*.sh                   # if you see "permission denied" when running scripts
./scripts/build_nomitri.sh install   # Conan: opencv, gtest, onnxruntime, onetbb (--build=missing)
./scripts/build_nomitri.sh build     # CMake configure + build
# or in one step:
./scripts/build_nomitri.sh all
```

Many Lightning images already have Conan; if `conan --version` works, skip `pip install conan`. If the image does not have `cmake` or a C++23 compiler, install them first (e.g. `apt-get update && apt-get install -y cmake build-essential` on Debian/Ubuntu-based images).

### 4. Run the CLI

**Mock backend (no model file):**

```bash
./build/apps/normitri-cli/normitri_cli --input data/images/sample_shelf.jpg
```

**ONNX backend:** Download a model and point the CLI at it:

```bash
./scripts/download_onnx_models.sh
./scripts/download_sample_images.sh
./build/apps/normitri-cli/normitri_cli --backend onnx \
  --model models/onnx-community__yolov10n/onnx/model.onnx \
  --input data/images/sample_shelf.jpg
```

**TensorRT backend (GPU instance only):** If the image has TensorRT and CUDA, the build will have enabled the TensorRT backend. Use a pre-built `.engine` file:

```bash
./build/apps/normitri-cli/normitri_cli --backend tensorrt \
  --model /path/to/model.engine \
  --input data/images/sample_shelf.jpg
```

### 5. Run tests

```bash
ctest --test-dir build
```

To run TensorRT backend tests (when built), set the engine path and run the vision tests:

```bash
export NORMITRI_TEST_TENSORRT_ENGINE=/path/to/model.engine
./build/tests/normitri_vision_tests
```

### 6. Tips

| Topic | Note |
|-------|------|
| **Paths** | Studio workspace is often under `/home` or a mounted volume; use `pwd` and paths relative to the repo root (e.g. `models/`, `data/images/`). |
| **Persistence** | Save your work (commit + push, or Lightning’s save). Stopping the Studio may free the machine; re-open and run `./scripts/build_nomitri.sh build` (or `all`) if needed. |
| **GPU** | Only use `--backend tensorrt` on a GPU machine type; the TensorRT backend requires CUDA and a GPU at runtime. |
| **No TensorRT in image** | If the Studio image has no TensorRT/CUDA, the project still builds; the TensorRT backend is simply omitted. Use `--backend onnx` or `--backend mock`. |

For multi-camera or multi-user parallelism on a GPU server, use the TBB-based runner and one pipeline (e.g. TensorRT backend) per camera; see [Threading and memory management](threading-and-memory-management.md#tbb-based-multi-camera-runner).

## Cross-Compilation and Hardware

For embedded or edge targets (e.g. ARM, Jetson), use a CMake toolchain file that sets `CMAKE_SYSTEM_NAME`, the C/C++ compilers, and sysroot. The same CMake options above apply; ensure the Conan profile matches the target if using Conan.
