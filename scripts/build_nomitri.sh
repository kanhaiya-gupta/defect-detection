#!/usr/bin/env bash
# build_nomitri.sh — Install dependencies and/or build Normitri (C++23, OpenCV, GTest)
#
# Usage:
#   ./scripts/build_nomitri.sh install   # Install deps (Conan: OpenCV, GTest); if GPU present, try TensorRT
#   ./scripts/build_nomitri.sh build     # Configure and build (uses Conan toolchain if present)
#   ./scripts/build_nomitri.sh all      # install + build
#
# Requires: Conan 2.x (for install), CMake 3.21+, C++23 compiler.
# Run from repo root or any directory (script locates repo root).
# TensorRT: if a GPU is detected and TensorRT is not found, the install step will try to install it (apt);
#           if that fails (e.g. no NVIDIA repo), the build continues without TensorRT.

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"

cd "$REPO_ROOT"

usage() {
  echo "Usage: $0 { install | build | all }"
  echo "  install  Install dependencies (Conan: OpenCV, GTest) into build/; if GPU present, try TensorRT"
  echo "  build   Configure and build Normitri (Release)"
  echo "  all     install then build"
  exit 1
}

# Returns 0 if trtexec or TensorRT libs are available (so we can build/use TensorRT).
tensorrt_available() {
  command -v trtexec &>/dev/null && return 0
  [[ -x /usr/bin/trtexec ]] && return 0
  for dir in /usr/local/TensorRT*/bin /opt/TensorRT*/bin; do
    [[ -x "${dir}/trtexec" ]] 2>/dev/null && return 0
  done
  # CMake finds by headers/libs; if nvinfer is there, backend can be built (trtexec may be separate)
  [[ -f /usr/include/NvInfer.h ]] 2>/dev/null && return 0
  [[ -f /usr/local/include/NvInfer.h ]] 2>/dev/null && return 0
  return 1
}

# Detect GPU (NVIDIA); return 0 if present.
gpu_available() {
  command -v nvidia-smi &>/dev/null && nvidia-smi -L &>/dev/null && return 0
  return 1
}

# If GPU is present and TensorRT is not, try to install TensorRT via apt (best-effort; skip if no apt/sudo).
try_install_tensorrt_if_gpu() {
  if ! gpu_available; then
    return 0
  fi
  if tensorrt_available; then
    echo "TensorRT (or trtexec) already available; skipping install."
    return 0
  fi
  echo "GPU detected; TensorRT not found. Attempting to install TensorRT (apt)..."
  if ! command -v apt-get &>/dev/null; then
    echo "  apt-get not available; skip. To use TensorRT, install it and add trtexec to PATH."
    return 0
  fi
  local apt_cmd="apt-get"
  if [[ -f /.dockerenv ]] && [[ $(id -u) -eq 0 ]]; then
    apt_cmd="apt-get"
  elif command -v sudo &>/dev/null; then
    apt_cmd="sudo apt-get"
  else
    echo "  No sudo and not root; skip. To use TensorRT, install it (e.g. apt install tensorrt) and add trtexec to PATH."
    return 0
  fi
  if $apt_cmd update &>/dev/null && $apt_cmd install -y --no-install-recommends tensorrt libnvinfer-dev libcudart-dev 2>/dev/null; then
    echo "  TensorRT installed. You can run ./scripts/build_tensorrt_engine.sh to build a .engine file."
  else
    echo "  TensorRT install failed (e.g. NVIDIA repo not configured). Build will continue without TensorRT."
    echo "  To enable later: add NVIDIA TensorRT repo and run 'apt install tensorrt libnvinfer-dev', or set TRTEXEC=/path/to/trtexec."
  fi
}

cmd_install() {
  if ! command -v conan &>/dev/null; then
    echo "Conan not found. Install Conan 2.x: https://conan.io"
    exit 1
  fi
  echo "Installing dependencies with Conan..."
  CONAN_EXTRA=""
  if [[ -f /.dockerenv ]]; then
    CONAN_EXTRA="-c tools.system.package_manager:mode=install"
  fi
  conan install . --output-folder="$BUILD_DIR" --build=missing $CONAN_EXTRA
  # If GPU present, try to install TensorRT so backend and build_tensorrt_engine.sh work
  try_install_tensorrt_if_gpu || true
  # With cmake_layout Conan puts the toolchain in build/Release/generators/
  if [[ -f "$BUILD_DIR/conan_toolchain.cmake" ]]; then
    echo "Done. Toolchain: $BUILD_DIR/conan_toolchain.cmake"
  elif [[ -f "$BUILD_DIR/build/Release/generators/conan_toolchain.cmake" ]]; then
    echo "Done. Toolchain: $BUILD_DIR/build/Release/generators/conan_toolchain.cmake"
  else
    echo "Done. Run '$0 build' to configure and build (toolchain will be auto-detected)."
  fi
}

cmd_build() {
  if ! command -v cmake &>/dev/null; then
    echo "CMake not found. Install CMake 3.21+."
    exit 1
  fi
  TOOLCHAIN=""
  if [[ -f "$BUILD_DIR/conan_toolchain.cmake" ]]; then
    TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$BUILD_DIR/conan_toolchain.cmake"
    echo "Using Conan toolchain ($BUILD_DIR/conan_toolchain.cmake)."
  elif [[ -f "$BUILD_DIR/build/Release/generators/conan_toolchain.cmake" ]]; then
    TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$BUILD_DIR/build/Release/generators/conan_toolchain.cmake"
    echo "Using Conan toolchain (cmake_layout)."
  else
    echo "No Conan toolchain found; using system OpenCV and GTest (run '$0 install' if needed)."
  fi
  echo "Configuring..."
  if [[ -n "${NORMITRI_CMAKE_ARGS:-}" ]]; then
    # Eval so quoted groups in NORMITRI_CMAKE_ARGS (e.g. -DCMAKE_CXX_FLAGS="...") stay as single args.
    eval "cmake -B \"$BUILD_DIR\" $TOOLCHAIN -DCMAKE_BUILD_TYPE=Release $NORMITRI_CMAKE_ARGS"
  else
    cmake -B "$BUILD_DIR" $TOOLCHAIN -DCMAKE_BUILD_TYPE=Release
  fi
  echo "Building..."
  cmake --build "$BUILD_DIR"
  echo "Done. Binary: $BUILD_DIR/apps/normitri-cli/normitri_cli (or normitri_cli.exe)"
}

case "${1:-}" in
  install) cmd_install ;;
  build)   cmd_build   ;;
  all)     cmd_install; cmd_build ;;
  *)       usage       ;;
esac
