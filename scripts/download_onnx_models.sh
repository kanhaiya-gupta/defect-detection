#!/usr/bin/env bash
# Download pre-trained ONNX detection models into models/.
# Run from repo root: ./scripts/download_onnx_models.sh [repo_id ...]
#
# Requires: pip install huggingface_hub (CLI: hf or huggingface-cli, or we use Python API)
# Example: ./scripts/download_onnx_models.sh
#          ./scripts/download_onnx_models.sh onnx-community/yolov10n onnxmodelzoo/ssd_mobilenet_v1_12

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MODELS_DIR="$REPO_ROOT/models"
mkdir -p "$MODELS_DIR"
cd "$REPO_ROOT"

# Prefer: hf (new CLI), then huggingface-cli, else Python API (works when CLI not on PATH, e.g. Windows conda)
HF_CMD=""
if command -v hf &>/dev/null; then
  HF_CMD="hf download"
elif command -v huggingface-cli &>/dev/null; then
  HF_CMD="huggingface-cli download"
elif python -c "import huggingface_hub" 2>/dev/null; then
  HF_CMD="python"
else
  echo "huggingface_hub not found. Install with: pip install huggingface_hub" >&2
  echo "Then run: hf auth login  # optional, for gated models" >&2
  exit 1
fi

# Default repos if no args: one small YOLO-style model
DEFAULT_REPOS=(
  "onnx-community/yolov10n"
  # "onnxmodelzoo/ssd_mobilenet_v1_12"  # uncomment to also fetch SSD-MobileNet
)

REPOS=("${@:-${DEFAULT_REPOS[@]}}")

for repo in "${REPOS[@]}"; do
  # Use repo name as subdir (e.g. onnx-community__yolov10n) to avoid slashes
  subdir="${repo//\//__}"
  dest="$MODELS_DIR/$subdir"
  echo "Downloading $repo to $dest ..."
  if [ "$HF_CMD" = "python" ]; then
    REPO_ID="$repo" LOCAL_DIR="$dest" python -c "
from huggingface_hub import snapshot_download
import os
snapshot_download(repo_id=os.environ['REPO_ID'], local_dir=os.environ['LOCAL_DIR'], local_dir_use_symlinks=False)
"
  else
    $HF_CMD "$repo" --local-dir "$dest"
  fi
  echo "  -> ONNX file(s) in $dest"
done

echo "Done. Use a model with: --model models/<subdir>/<model>.onnx"
echo "  e.g. --model models/onnx-community__yolov10n/model.onnx  (exact name depends on the repo)"
