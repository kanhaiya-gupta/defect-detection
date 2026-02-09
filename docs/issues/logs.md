# Issue Log: Build & Docker Problems and Solutions

This document records problems encountered when building this project (especially in WSL and Docker) and how they were fixed. Use it to explain issues to others or to avoid repeating the same fixes.

---

## 1. Shell script line endings (WSL)

### Problem

When running `bash scripts/build_nomitri.sh` or `bash scripts/docker_nomitri.sh` in WSL (or on Linux), you may see:

```text
$'\r': command not found
```

or

```text
invalid option
```

### Cause

Scripts were saved or committed with Windows line endings (CRLF: `\r\n`). Bash on Linux/WSL treats `\r` as part of the command, which leads to "command not found" or "invalid option" errors.

### Solution

1. **Enforce LF in Git**  
   In `.gitattributes` we have:
   ```gitattributes
   *.sh text eol=lf
   ```
   So all `.sh` files are checked out with LF. New clones and checkouts will get correct line endings.

2. **Fix existing files**  
   If you already have CRLF scripts (e.g. before the rule was added), normalize them:
   ```bash
   sed -i 's/\r$//' scripts/*.sh
   ```

3. **Editor**  
   Configure your editor to use LF for shell scripts when working on this repo.

---

## 2. Conan default profile missing (Docker)

### Problem

Inside the Docker image, running the build fails with something like:

```text
default build profile doesn't exist
```

or Conan complains that no profile is set.

### Cause

The image had Conan installed but no Conan profile. Profiles are normally created on the host with `conan profile detect` and live in the user’s Conan home; the image had a fresh Conan install and no profile.

### Solution

In the **Dockerfile**, after installing Conan, we run:

```dockerfile
RUN conan profile detect --force
```

This creates the default host (and build) profile inside the image (e.g. `default`), so any `conan install` or `conan build` that runs in the container has a valid profile.

---

## 3. Conan system requirements in Docker (vaapi/system, etc.)

### Problem

During `conan install` inside Docker you see:

```text
ERROR: vaapi/system: Error in system_requirements() method, line 38
    apt.install(["libva-dev"], update=True, check=True)
ConanException: System requirements: 'libva-dev' are missing but can't install because
tools.system.package_manager:mode is 'check'. Please update packages manually or set
'tools.system.package_manager:mode' to 'install' in the [conf] section of the profile,
or in the command line using '-c tools.system.package_manager:mode=install'
```

Similar errors can appear for other system packages (e.g. from `vdpau/system` or other `*_system` recipes).

### Cause

- OpenCV (and its Conan dependency tree) pulls in Conan “system” packages such as `vaapi/system` and `vdpau/system`.
- Those recipes call `system_requirements()` and try to install packages like `libva-dev` via the system package manager (e.g. `apt`).
- By default Conan uses `tools.system.package_manager:mode=check`: it only checks that the packages exist and does **not** install them. So if the Docker image doesn’t have those packages, the install fails.

### Solution (two parts)

**A. Pre-install common system packages in the image**

In the **Dockerfile**, we install the system libraries and `-dev` packages that these Conan recipes typically need, for example:

- `libva-dev`, `libvdpau-dev` (vaapi/vdpau)
- `libxkbcommon-dev`, `libwayland-dev`, and other X/Wayland/Mesa/audio deps as needed (e.g. `libxrandr-dev`, `libgl1-mesa-dev`, `libpulse-dev`, etc.)

That way most `system_requirements()` are satisfied without Conan having to run the package manager.

**B. Allow Conan to install missing system packages in Docker**

In **`scripts/build_nomitri.sh`**, when the script runs inside Docker (detected via `/.dockerenv`), we pass:

```bash
-c tools.system.package_manager:mode=install
```

to `conan install`. So if a recipe still requests a system package that isn’t in the Dockerfile, Conan is allowed to install it via `apt` instead of failing with “mode is 'check'”.

Together, (A) keeps the image predictable and (B) avoids failures when a new dependency adds a system requirement we didn’t pre-install.

---

## 4. “No compatible configuration found” for some Conan packages

### Problem

During dependency resolution you see messages like:

```text
libtiff/4.6.0: No compatible configuration found
openexr/3.2.3: No compatible configuration found
opencv/4.9.0: No compatible configuration found
```

and Conan lists configurations with different `compiler.cppstd` (e.g. 11, 14, 17, 20) but none matching your profile (e.g. `gnu17` or `gnu23`).

### Cause

Conan Center has **prebuilt binaries** only for certain configurations (compiler, version, cppstd, etc.). Your profile (e.g. GCC 13 with `gnu17` or `gnu23`) may not have prebuilt binaries for every package. Conan then reports “No compatible configuration found” for those packages and, with `--build=missing`, will **build them from source** instead.

### Solution

- This is **expected** when using a compiler or C++ standard that doesn’t have prebuilt binaries for all dependencies. Conan will build the missing packages from source; the first run can be slow.
- No change is required if the rest of the install succeeds (system requirements are satisfied, etc.).
- Optionally, if build time is a concern, you can try a profile that uses a C++ standard with more prebuilt binaries (e.g. 17) for dependencies, while still building your own project with C++23 in CMake. For many setups, building opencv and a few others from source once is acceptable.

---

## 5. GitHub rate limit (Error 429) when Conan downloads sources

### Problem

During `conan install`, when Conan builds a package from source (e.g. **gtest/1.14.0**), it may download the recipe’s source from GitHub. You then see:

```text
gtest/1.14.0: WARN: network: Error 429 downloading file https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
...
ConanException: Error 429 downloading file https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
```

The same can happen for other recipes that fetch sources from GitHub (e.g. when building opencv or other packages from source).

### Cause

**GitHub rate limits** unauthenticated API and archive downloads (e.g. 60 requests per hour per IP). In CI or Docker, many requests in a short time can hit this limit and trigger **HTTP 429 Too Many Requests**.

### Solution

1. **Retry later**  
   Wait for the rate limit to reset (e.g. after an hour) and run `conan install` again. The Conan cache will keep already-downloaded packages, so only the failed one(s) will be fetched.

2. **Use a GitHub token (recommended in CI/Docker)**  
   Create a [GitHub personal access token](https://github.com/settings/tokens) (no special scopes needed for public repo tarballs). Then pass it so Conan or the download layer can use it:
   - **Conan 2** can use `GITHUB_TOKEN` for GitHub downloads in some recipes. Before running the script:
     ```bash
     export GITHUB_TOKEN=ghp_your_token_here
     bash scripts/docker_nomitri.sh
     ```
   - If running **inside** Docker, pass the token into the container (e.g. `-e GITHUB_TOKEN=...` in `docker run`). In `scripts/docker_nomitri.sh` you can add `-e GITHUB_TOKEN` when the variable is set so the container sees it.

3. **Use system GTest (Docker-only workaround)**  
   To avoid downloading GTest from GitHub in Docker, you can install GTest via the system package manager in the image (`apt install libgtest-dev`) and then use a Conan profile or conanfile that uses the system GTest instead of the Conan one. This requires a bit more configuration (e.g. a profile with `gtest/*:system=True` or a different conanfile for Docker) and is optional if the token or retry approach works.

### Quick fix to try first

- **Retry:** run `bash scripts/docker_nomitri.sh` again after some time.
- **With token:** `export GITHUB_TOKEN=ghp_xxx` then run the script; if the script runs Conan inside Docker, ensure the script passes `GITHUB_TOKEN` into the container (see script for `-e` env vars).

---

## Quick reference

| Symptom | Likely cause | Where to fix |
|--------|----------------|--------------|
| `$'\r': command not found` / `invalid option` on scripts | CRLF line endings | `.gitattributes` (`*.sh text eol=lf`), `sed -i 's/\r$//' scripts/*.sh` |
| “default build profile doesn’t exist” in Docker | No Conan profile in image | Dockerfile: `RUN conan profile detect --force` |
| “libva-dev are missing… mode is 'check'” (or similar) | System packages not installed; Conan won’t install by default | Dockerfile: install `libva-dev`, etc.; build script: in Docker add `-c tools.system.package_manager:mode=install` |
| “No compatible configuration found” for libtiff/opencv/openexr | No prebuilt binaries for your profile | Expected; Conan builds from source with `--build=missing` |
| **Error 429** downloading gtest/opencv (etc.) from GitHub | GitHub rate limiting | Retry later; or set `GITHUB_TOKEN` and pass it into Docker if needed (see [§5](#5-github-rate-limit-error-429-when-conan-downloads-sources)) |
| **libwebp::libwebp target not found** when building OpenCV with Conan | FFmpeg CMake files reference libwebp; OpenCV doesn’t see it | Set `opencv/*:with_ffmpeg=False` in conanfile.txt (see [§6](#6-opencv-conan-build-ffmpeg--libwebp-target-not-found)) |
| **"Options should be specified as 'pkg/*:option=value'"** in conanfile.txt | Conan 2 requires wildcard form for options | Use `opencv/*:with_ffmpeg=False` not `opencv:with_ffmpeg=False` (see [§6](#6-opencv-conan-build-ffmpeg--libwebp-target-not-found)) |
| **"No Conan toolchain found"** then **"Could not find OpenCV"** after install | Toolchain/generators under `build/build/Release/generators/` with cmake_layout | Build script checks both toolchain paths; CMakeLists.txt auto-detects generators and sets CMAKE_PREFIX_PATH (see [§7](#7-cmake-could-not-find-opencv-after-conan-install-toolchain-not-used)) |

---

## 6. OpenCV Conan build: FFmpeg / libwebp target not found

### Problem

When Conan builds OpenCV from source, CMake configure can fail with:

```text
CMake Error at .../ffmpeg-Target-release.cmake:231 (set_property):
  The link interface of target "ffmpeg_ffmpeg_avcodec_DEPS_TARGET" contains:
    libwebp::libwebp
  but the target was not found.
```

### Cause

OpenCV’s Conan recipe enables **FFmpeg** by default (for the videoio module). The Conan-generated FFmpeg CMake files reference the `libwebp::libwebp` target, but that target is not visible in the OpenCV build context, so the configure step fails.

### Solution

The project only uses OpenCV for **image processing** (resize, normalize, color conversion) via `core` and `imgproc`; it does **not** use video I/O. Disable FFmpeg in the Conan options so the videoio module is built without FFmpeg and the broken dependency chain is avoided.

In **`conanfile.txt`**:

```ini
[options]
opencv/*:with_ffmpeg=False
```

**Conan 2 format:** Options in `conanfile.txt` must use the **`pkg/*:option=value`** form. If you use `opencv:with_ffmpeg=False` (no slash), Conan will report: *"Options should be specified as 'pkg/*:option=value'"*. Use **`opencv/*:with_ffmpeg=False`** (with the `/*` wildcard).

Then run `conan install` again (and rebuild the image if using Docker, so the new option is used). If you later need FFmpeg-based video I/O, you would need to fix the Conan/FFmpeg/libwebp dependency wiring or use system OpenCV with FFmpeg support.

---

## 7. CMake "Could not find OpenCV" after Conan install (toolchain not used)

### Problem

After `conan install` completes successfully (OpenCV and GTest are built and packaged), the **build** step fails with:

```text
No Conan toolchain found; using system OpenCV and GTest (run './scripts/build_nomitri.sh install' if needed).
Configuring...
CMake Error at CMakeLists.txt:34 (find_package):
  By not providing "FindOpenCV.cmake" in CMAKE_MODULE_PATH this project has
  asked CMake to find a package configuration file provided by "OpenCV", but
  CMake did not find one.
```

So Conan did install OpenCV and generated a toolchain, but the build step did not use it.

### Cause

The project uses **`[layout] cmake_layout`** in `conanfile.txt`. With that layout, Conan writes the toolchain and generators to **`build/build/Release/generators/`** (e.g. `conan_toolchain.cmake` there), not to `build/conan_toolchain.cmake`. The build script was only looking for `build/conan_toolchain.cmake`, so it never found the Conan toolchain and ran CMake without it; CMake then looked for system OpenCV and failed.

### Solution

**`scripts/build_nomitri.sh`** was updated so that the **build** step looks for the Conan toolchain in both places:

1. **`build/conan_toolchain.cmake`** (no layout or older Conan behavior)
2. **`build/build/Release/generators/conan_toolchain.cmake`** (with `cmake_layout`)

Whichever exists is used as `CMAKE_TOOLCHAIN_FILE`, so `find_package(OpenCV)` resolves to Conan’s package. No change to `conanfile.txt` or Docker is required; run `./scripts/build_nomitri.sh build` or `./scripts/build_nomitri.sh all` again after pulling the script fix.

### Update: CMake auto-detects Conan generators

In some environments the toolchain was passed but `find_package(OpenCV)` still failed (e.g. path resolution). **`CMakeLists.txt`** was updated to **auto-detect** the Conan generators directory before `find_package(OpenCV)`:

- If **`${CMAKE_BINARY_DIR}/build/Release/generators/OpenCVConfig.cmake`** exists (cmake_layout), that directory is prepended to **`CMAKE_PREFIX_PATH`**.
- If **`${CMAKE_BINARY_DIR}/OpenCVConfig.cmake`** exists (flat layout), that directory is prepended instead.

So CMake finds OpenCV and GTest from the Conan-generated configs even when the script does not pass `CMAKE_PREFIX_PATH`. You can run `cmake -B build` (or use an IDE) after `conan install . --output-folder=build` and the build will succeed without extra `-D` options.

---

*Last updated to reflect fixes applied in the repo (Dockerfile, build scripts, .gitattributes, conanfile options, Conan toolchain path for cmake_layout, CMakeLists auto-detect of Conan generators).*
