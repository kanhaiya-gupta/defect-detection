#!/usr/bin/env bash
# build_nomitri.sh — Install dependencies and/or build Normitri (C++23, OpenCV, GTest)
#
# Usage:
#   ./scripts/build_nomitri.sh install   # Install deps (Conan: OpenCV, GTest)
#   ./scripts/build_nomitri.sh build     # Configure and build (uses Conan toolchain if present)
#   ./scripts/build_nomitri.sh all       # install + build
#
# Requires: Conan 2.x (for install), CMake 3.21+, C++23 compiler.
# Run from repo root or any directory (script locates repo root).

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"

cd "$REPO_ROOT"

usage() {
  echo "Usage: $0 { install | build | all }"
  echo "  install  Install dependencies (Conan: OpenCV, GTest) into build/"
  echo "  build    Configure and build Normitri (Release)"
  echo "  all      install then build"
  exit 1
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
