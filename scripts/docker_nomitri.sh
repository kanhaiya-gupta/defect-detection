#!/usr/bin/env bash
# docker_nomitri.sh — Build and run Normitri inside Docker (no Conan/OpenCV on host)
#
# Usage:
#   ./scripts/docker_nomitri.sh              # default: all (install deps + build)
#   ./scripts/docker_nomitri.sh all           # install + build
#   ./scripts/docker_nomitri.sh install       # Conan install only
#   ./scripts/docker_nomitri.sh build         # CMake configure + build only
#   ./scripts/docker_nomitri.sh test          # run tests (ctest)
#   ./scripts/docker_nomitri.sh run [args]    # run normitri_cli (pass args to CLI)
#   ./scripts/docker_nomitri.sh shell         # interactive bash in container
#   ./scripts/docker_nomitri.sh image         # (re)build Docker image only
#
# Requires: Docker. Run from repo root (script will cd to repo root).
# On Windows: use WSL or Git Bash so that $(pwd) and the script work.
#sed -i 's/\r$//' scripts/*.sh
#bash scripts/docker_nomitri.sh

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_NAME="${NORMITRI_DOCKER_IMAGE:-normitri-builder}"
SRC_MOUNT="${1:-}"

cd "$REPO_ROOT"

VOLUME_MOUNT="$(pwd):/src"
if [[ "$(uname -s)" == "MSYS"* ]] || [[ "$(uname -s)" == "MINGW"* ]]; then
  if command -v cygpath &>/dev/null; then
    VOLUME_MOUNT="$(cygpath -w "$(pwd)" | sed 's/\\/\//g'):/src"
  fi
fi

run_in_docker() {
  local -a docker_opts=(--rm -v "${VOLUME_MOUNT}" -w /src)
  [[ -n "${GITHUB_TOKEN:-}" ]] && docker_opts+=(-e GITHUB_TOKEN)
  docker run "${docker_opts[@]}" "$IMAGE_NAME" "$@"
}

usage() {
  echo "Usage: $0 [command] [args...]"
  echo ""
  echo "Commands:"
  echo "  (none)    same as 'all'"
  echo "  all       install deps (Conan) + configure + build"
  echo "  install   Conan install only"
  echo "  build     CMake configure + build only"
  echo "  test      run tests (ctest)"
  echo "  run [args]  run normitri_cli (e.g. $0 run --help)"
  echo "  shell     interactive bash in container"
  echo "  image     (re)build Docker image only"
  echo ""
  echo "Set NORMITRI_DOCKER_IMAGE to use a different image name (default: normitri-builder)."
  echo "Set GITHUB_TOKEN if you hit GitHub 429 rate limits when Conan downloads sources (see docs/issues/logs.md)."
  exit 1
}

cmd_image() {
  echo "Building Docker image: $IMAGE_NAME"
  docker build -t "$IMAGE_NAME" "$REPO_ROOT"
  echo "Done."
}

cmd_install() {
  ensure_image
  echo "Running: install (Conan)..."
  run_in_docker ./scripts/build_nomitri.sh install
}

cmd_build() {
  ensure_image
  echo "Running: build..."
  run_in_docker ./scripts/build_nomitri.sh build
}

cmd_all() {
  ensure_image
  echo "Running: install + build..."
  run_in_docker ./scripts/build_nomitri.sh all
}

cmd_test() {
  ensure_image
  echo "Running tests..."
  run_in_docker ctest --test-dir build --output-on-failure
}

cmd_run() {
  ensure_image
  run_in_docker bash -c 'if [ -f build/apps/normitri-cli/normitri_cli ]; then exec build/apps/normitri-cli/normitri_cli "$@"; else exec build/normitri_cli "$@"; fi' -- "$@"
}

cmd_shell() {
  ensure_image
  echo "Starting shell in container (workdir: /src). Exit with Ctrl+D or 'exit'."
  docker run --rm -it -v "${VOLUME_MOUNT}" -w /src "$IMAGE_NAME" bash
}

ensure_image() {
  if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "Image $IMAGE_NAME not found. Building..."
    cmd_image
  fi
}

case "${1:-all}" in
  image)   cmd_image   ;;
  install) cmd_install ;;
  build)   cmd_build   ;;
  all)     cmd_all     ;;
  test)    cmd_test    ;;
  run)     shift; cmd_run "$@" ;;
  shell)   cmd_shell   ;;
  -h|--help|help) usage ;;
  *)       usage       ;;
esac
