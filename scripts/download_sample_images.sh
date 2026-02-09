#!/usr/bin/env bash
# Download defect-detection–relevant sample images (retail/shelf) into data/images/.
# Run from repo root: ./scripts/download_sample_images.sh

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DATA_DIR="$REPO_ROOT/data/images"
mkdir -p "$DATA_DIR"

# Retail/supermarket shelf image (relevant for wrong-item, quantity, expiry, process-error detection).
# Source: Wikimedia Commons, "Supermarket shelves.jpg" (CC BY-SA 4.0), 640px width for reasonable size.
SAMPLE_URL="https://upload.wikimedia.org/wikipedia/commons/thumb/4/41/Supermarket_shelves.jpg/640px-Supermarket_shelves.jpg"
SAMPLE_PATH="$DATA_DIR/sample_shelf.jpg"

if command -v curl &>/dev/null; then
  echo "Downloading retail shelf sample to $SAMPLE_PATH ..."
  curl -sL -o "$SAMPLE_PATH" "$SAMPLE_URL"
elif command -v wget &>/dev/null; then
  echo "Downloading retail shelf sample to $SAMPLE_PATH ..."
  wget -q -O "$SAMPLE_PATH" "$SAMPLE_URL"
else
  echo "Error: need curl or wget to download sample image." >&2
  exit 1
fi

echo "Done. Run: ./build/apps/normitri-cli/normitri_cli --input data/images/sample_shelf.jpg"
