#!/usr/bin/env bash
# Build a TensorRT .engine file from an ONNX model (for use with --backend tensorrt).
# Run from repo root. Requires: TensorRT installed, GPU, and trtexec on PATH (or set TRTEXEC).
#
# Usage:
#   ./scripts/build_tensorrt_engine.sh
#     Uses default ONNX: models/onnx-community__yolov10n/onnx/model.onnx
#     Writes: models/onnx-community__yolov10n/engine/model.engine
#   ./scripts/build_tensorrt_engine.sh /path/to/model.onnx [/path/to/output.engine]
#
# If you don't have TensorRT/GPU, use the ONNX backend with the .onnx file instead.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# Default ONNX path (from download_onnx_models.sh default)
DEFAULT_ONNX="$REPO_ROOT/models/onnx-community__yolov10n/onnx/model.onnx"

ONNX_PATH="${1:-$DEFAULT_ONNX}"
if [ ! -f "$ONNX_PATH" ]; then
  echo "ONNX file not found: $ONNX_PATH" >&2
  echo "Download one first, e.g.: ./scripts/download_onnx_models.sh" >&2
  exit 1
fi

# Output path: same dir as ONNX but ../engine/model.engine, or second arg
if [ -n "$2" ]; then
  ENGINE_PATH="$2"
else
  ONNX_DIR="$(dirname "$ONNX_PATH")"
  ENGINE_DIR="$(dirname "$ONNX_DIR")/engine"
  mkdir -p "$ENGINE_DIR"
  ENGINE_PATH="$ENGINE_DIR/model.engine"
fi

# Find trtexec (TensorRT): TRTEXEC, TRT_DIR/TensorRT_ROOT, PATH, then common install locations
TRTEXEC="${TRTEXEC:-}"
if [ -z "$TRTEXEC" ]; then
  if command -v trtexec &>/dev/null; then
    TRTEXEC="trtexec"
  elif [ -n "${TRT_DIR:-}" ] && [ -x "${TRT_DIR}/bin/trtexec" ]; then
    TRTEXEC="${TRT_DIR}/bin/trtexec"
  elif [ -n "${TensorRT_ROOT:-}" ] && [ -x "${TensorRT_ROOT}/bin/trtexec" ]; then
    TRTEXEC="${TensorRT_ROOT}/bin/trtexec"
  elif [ -x "/usr/bin/trtexec" ]; then
    TRTEXEC="/usr/bin/trtexec"
  else
    # Common TensorRT install paths (e.g. NVIDIA containers, /opt, /usr/local)
    for dir in /usr/local/TensorRT*/bin /opt/TensorRT*/bin /usr/local/cuda*/bin; do
      if [ -x "${dir}/trtexec" ] 2>/dev/null; then
        TRTEXEC="${dir}/trtexec"
        break
      fi
    done
    # Tar install in repo (e.g. TensorRT-10.15.1.29)
    if [ -z "$TRTEXEC" ] && [ -x "$REPO_ROOT/TensorRT-10.15.1.29/bin/trtexec" ]; then
      TRTEXEC="$REPO_ROOT/TensorRT-10.15.1.29/bin/trtexec"
    fi
  fi
fi
if [ -z "$TRTEXEC" ] || ! [ -x "$TRTEXEC" ]; then
  echo "trtexec not found. TensorRT is required to build a .engine file." >&2
  echo "  - Install TensorRT on this machine and add its bin/ to PATH, or set TRTEXEC=/path/to/trtexec" >&2
  echo "  - If this environment has no GPU/TensorRT, use the ONNX backend instead: --backend onnx --model <path/to/model.onnx>" >&2
  exit 1
fi

echo "Building TensorRT engine from $ONNX_PATH"
echo "  -> $ENGINE_PATH"
"$TRTEXEC" --onnx="$ONNX_PATH" --saveEngine="$ENGINE_PATH" --fp16
echo "Done. Run with: --backend tensorrt --model $ENGINE_PATH"
