# Sample images for the defect-detection pipeline

Put input images here (e.g. retail/shelf JPEG/PNG from camera or file) and run the CLI with `--input data/images/<file>`. The pipeline is aimed at **defect detection** (wrong item, wrong quantity, expiry/quality, process error) in retail/shopping scenes.

## Quick start

1. **Download a defect-detection–relevant sample** (optional):

   ```bash
   ./scripts/download_sample_images.sh
   ```

   This fetches a retail supermarket-shelf image into this folder (`sample_shelf.jpg`).

2. **Run the pipeline** on it:

   ```bash
   ./build/apps/normitri-cli/normitri_cli --input data/images/sample_shelf.jpg
   ```

Without `--input`, the CLI uses a synthetic in-memory frame. With a real shelf/image, the pipeline runs the same stages (resize, normalize, color convert, inference); with the default mock inference backend the reported defects are still synthetic until you plug in a real model.

## Notes

- Image files in this directory are ignored by Git (see root `.gitignore`) so the repo stays small; only this README is tracked.
- Supported formats: whatever OpenCV can read (e.g. JPEG, PNG, BMP).
