# Building the Project

## Prerequisites

- **Compiler** — GCC 13+, Clang 16+, or MSVC 2022 17.6+ with C++23 support
- **CMake** — 3.21 or newer
- **OpenCV** — **Required** (vision stages: resize, normalize, color conversion).
- **GTest** — **Required** when building tests (`NORMITRI_BUILD_TESTS=ON`, default). Provide via Conan or system packages.
- **Optional** — Conan 2.x to supply both OpenCV and GTest; otherwise install both system-wide. Or use **Docker** (no Conan/OpenCV install on host).

## Install Conan on the host (if not using Docker)

```bash
pip install conan
# or with --user if you prefer: pip install --user conan
# Ensure `conan` is on PATH (e.g. restart terminal or source ~/.profile)
```

Then run `./scripts/build_nomitri.sh install` and `./scripts/build_nomitri.sh build` as below.

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
./scripts/build_nomitri.sh install   # Install OpenCV + GTest via Conan
./scripts/build_nomitri.sh build     # Configure and build (uses Conan toolchain if present)
# or
./scripts/build_nomitri.sh all       # install then build
```

The script uses Conan for dependencies and writes the toolchain to `build/conan_toolchain.cmake`; `build` then uses it automatically.

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

## Cross-Compilation and Hardware

For embedded or edge targets (e.g. ARM, Jetson), use a CMake toolchain file that sets `CMAKE_SYSTEM_NAME`, the C/C++ compilers, and sysroot. The same CMake options above apply; ensure the Conan profile matches the target if using Conan.
