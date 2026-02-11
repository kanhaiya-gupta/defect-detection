# Models

This folder is for **ONNX model files** (and, in the future, TensorRT engine files) used by the defect-detection pipeline.

## Where to get a pre-trained ONNX detector

You can use a pre-trained detector from:

- **Hugging Face (ONNX)**  
  - [ONNX Community](https://huggingface.co/onnx-community) — e.g. **YOLOv10** ([yolov10n](https://huggingface.co/onnx-community/yolov10n), [YOLOv10](https://huggingface.co/onnx-community/YOLOv10)), **RF-DETR** ([rfdetr_medium-ONNX](https://huggingface.co/onnx-community/rfdetr_medium-ONNX)).  
  - Browse object-detection models: [Hugging Face — object-detection + ONNX](https://huggingface.co/models?pipeline_tag=object-detection&library=onnx).  
  - Download the `.onnx` file and place it in `models/` (or use the path when running the CLI).  
  - **Helper script:** From repo root run **`./scripts/download_onnx_models.sh`** to fetch default ONNX detection models into `models/` (requires `pip install huggingface_hub`). For a specific repo: `./scripts/download_onnx_models.sh onnx-community/yolov10n`.
- **ONNX Model Zoo (on Hugging Face)**  
  - The official [ONNX Model Zoo](https://github.com/onnx/models) detection models are mirrored on Hugging Face under [onnxmodelzoo](https://huggingface.co/onnxmodelzoo), e.g. **SSD-MobileNetV1** ([ssd_mobilenet_v1_12](https://huggingface.co/onnxmodelzoo/ssd_mobilenet_v1_12)), **Mask R-CNN** ([MaskRCNN-10](https://huggingface.co/onnxmodelzoo/MaskRCNN-10)).  
  - Note: the legacy ONNX Model Zoo site (onnx.ai) no longer serves LFS downloads; use the Hugging Face mirrors.

**Compatibility:** Our ONNX backend expects a model with (at least) three outputs: **boxes**, **scores**, and **class IDs**. YOLO-style or SSD-style detection models usually match; if a model has a different output layout (e.g. a single combined tensor), you may need to adapt the backend or export a model that matches. See [Inference contract](../docs/inference-contract.md) and the ONNX backend implementation.

## TensorRT engine: build from ONNX (no download)

Unlike ONNX, **TensorRT `.engine` files are not downloaded**. They are **built on the target GPU** from an ONNX model, because the engine is specific to the GPU and TensorRT version.

**Workflow:**

1. **Download an ONNX model** (as above): e.g. `./scripts/download_onnx_models.sh` → `models/onnx-community__yolov10n/onnx/model.onnx`.
2. **Build the engine** on a machine that has TensorRT and a GPU: run **`./scripts/build_tensorrt_engine.sh`** (or use `trtexec` directly; see script and [Building](../docs/building.md#building-with-tensorrt-optional-gpu)). The script uses the downloaded ONNX and writes a `.engine` file into `models/` (e.g. `models/onnx-community__yolov10n/engine/model.engine`).
3. **Run the CLI** with `--backend tensorrt --model models/.../engine/model.engine`.

If you don’t have a GPU or TensorRT, use the **ONNX** backend with the same `.onnx` file (CPU or ONNX Runtime with CUDA). No `.engine` is required for ONNX.

**Reproducing TensorRT engine and run:** For step-by-step env vars, build, and run (and troubleshooting e.g. TensorRT 10 API or missing libs), see [TensorRT: Reproducible setup and troubleshooting](../docs/building.md#tensorrt-reproducible-setup-and-troubleshooting) in the building guide. Minimal repro from repo root:

```bash
export TRT_DIR="$(pwd)/TensorRT-10.15.1.29"
export PATH="$TRT_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$TRT_DIR/lib:$LD_LIBRARY_PATH"
./scripts/build_tensorrt_engine.sh
# Then run (same LD_LIBRARY_PATH):
./build/apps/normitri-cli/normitri_cli --backend tensorrt --model models/onnx-community__yolov10n/engine/model.engine --input data/images/defective_fruits.jpg
```

## What to put here

- **ONNX models** (`.onnx`) — e.g. `detector.onnx`, or a file downloaded from Hugging Face / the model zoo. Or place the file(s) your ML team provides or that you export from your training pipeline.
- **TensorRT engines** (`.engine`) — built from an ONNX model on the target GPU (see above); do not commit large binaries.
- You can have **one or many** files (e.g. one per product category or per customer). Each pipeline uses one model at a time; the application chooses which model (and thus which file) to use per camera or customer.

## How they are used

- **Config:** Set `model_path=models/detector.onnx` (or another path under `models/`) in your config file, then run with `--config <your_config>`.
- **CLI:** From the repo root, run e.g.  
  `normitri_cli --backend onnx --model models/detector.onnx --input data/images/sample_shelf.jpg`  
  or with Docker:  
  `./scripts/docker_nomitri.sh run --backend onnx --model /src/models/detector.onnx --input data/images/sample_shelf.jpg`
- Paths can be **relative** (to the process current working directory) or **absolute**. When using Docker, the repo is mounted at `/src`, so use `/src/models/...` for paths inside the container.

## Do not commit large binaries (optional)

If your models are large, add to `.gitignore` (in the repo root) for example:

```
models/*.onnx
models/*.engine
```

Then copy or download models into `models/` at build or deploy time instead of committing them.

For more on model paths, config, and using many models (e.g. per customer), see [Implementation plan — Models](../docs/implementation_plan.md#phase-1-onnx-runtime-backend-cpu-first).
