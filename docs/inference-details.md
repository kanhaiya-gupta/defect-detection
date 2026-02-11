# Inference Details: ONNX, Training, and Runtimes

This document explains the concepts and division of labour around inference in Normitri: what ONNX is, who trains models, what software runs them, and how TensorRT relates to ONNX Runtime.

---

## ONNX: format vs software

**ONNX (Open Neural Network Exchange)** is not an application you run — it is:

- **A format**: an open standard for representing machine learning models (graphs, operators, data types). A **`.onnx` file** is a model saved in this format.
- **An ecosystem**: tools and runtimes that produce or consume that format.

The **software** that actually **loads and runs** `.onnx` files is a **runtime**. In this project we use **ONNX Runtime**.

| Term | Meaning |
|------|--------|
| **ONNX** | The format / specification; `.onnx` model files. |
| **ONNX Runtime** | The software (library) that executes ONNX models. Microsoft-led, cross-platform. |

So: ONNX = format; **ONNX Runtime** = the program that runs that format. Normitri’s “ONNX backend” depends on **ONNX Runtime** to handle `.onnx` files.

---

## Do we train ONNX models?

**No.** Training is done in frameworks such as **PyTorch** or **TensorFlow**. After training, the model is **exported** to the ONNX format (e.g. a single `.onnx` file). ONNX is only a way to **store and exchange** a trained model; you do not train “in ONNX.”

Normitri (and ONNX Runtime) only **run** the model: load the `.onnx` file, feed in preprocessed frames, and get back detections. No training happens in this pipeline.

---

## Division of labour: ML team vs application

Typical split:

| Role | Responsibility |
|------|----------------|
| **ML / data science** | Train the model (PyTorch, TensorFlow, etc.) on your data. Export it to **ONNX** (a `.onnx` file). Hand off the file and a short **contract**: input size (e.g. 640×640), layout (NCHW/NHWC), value range (e.g. 0–1), and **class IDs** (which index corresponds to which defect type). |
| **Application (Normitri)** | Use the ONNX backend with that file. Configure preprocessing (resize, normalize) to match the contract. Map the model’s class IDs to `DefectKind` in `DefectDecoder` (e.g. class 0 → WrongItem, 1 → WrongQuantity). Run inference in the pipeline. |

So: **ML people train and export → give you the `.onnx` file and the contract; you integrate and run it** in the C++ pipeline. No training in Normitri.

---

## Do we need to install “ONNX software”?

Yes — to **run** `.onnx` files you need a runtime. In this project that runtime is **ONNX Runtime**.

It is already a dependency of the project:

- **conanfile.txt**: `onnxruntime/1.18.1` — Conan fetches it.
- **CMakeLists.txt**: `find_package(onnxruntime)` and link `onnxruntime::onnxruntime`.

When you run:

```bash
./scripts/build_nomitri.sh install   # or docker_nomitri.sh install
./scripts/build_nomitri.sh build
```

Conan installs the ONNX Runtime libraries and the build links against them. You do not need to install “ONNX” or “ONNX Runtime” separately; it is pulled in with the rest of the dependencies. The mock backend does not require ONNX Runtime; the ONNX backend does.

---

## TensorRT vs ONNX Runtime

Both are **inference runtimes**: they load a model and run it to produce predictions. They do the **same kind of job**, but they are **different products** with different trade-offs.

| | **ONNX Runtime** | **TensorRT** |
|---|------------------|--------------|
| **Vendor** | Microsoft-led, open source | NVIDIA |
| **Runs** | `.onnx` models directly | ONNX (imported and optimized) or TensorRT “engine” files |
| **Hardware** | CPU, NVIDIA GPU (CUDA), and others | **NVIDIA GPUs only** |
| **Typical use** | Portable deployment, one format everywhere | Maximum performance on NVIDIA GPUs |
| **Setup** | One library, cross-platform | Requires CUDA, cuDNN, TensorRT SDK (NVIDIA stack) |

Summary:

- **Same role**: both execute a model (inference).
- **Different products**: different vendors, portability, and optimisation focus.
- **TensorRT** is for when you deploy on NVIDIA GPUs and want the highest throughput or lowest latency; **ONNX Runtime** is for portability and simpler integration (including CPU and non-NVIDIA environments).

In Normitri, the **ONNX backend** uses ONNX Runtime to run `.onnx` files. A future **TensorRT backend** would do the same job (inference) but use TensorRT instead, mainly for NVIDIA GPU performance. See [Implementation plan](implementation_plan.md) for Phase 2 (optional TensorRT backend).

---

## Many-class models (e.g. thousands of products)

ML models for a shop can have **many classes** — hundreds or thousands of products (SKUs). Normitri is designed to handle that.

- **No fixed class limit**: `InferenceResult` uses `std::vector` for `boxes`, `scores`, and `class_ids`; size is driven by the model’s output. `ClassToDefectKindMap` is a `std::vector<DefectKind>`: you pass one entry per model class (e.g. 10 000 entries for 10 000 products). Memory cost is small (one enum value per class).
- **Defect kind vs product ID**: The pipeline keeps **defect type** small (e.g. WrongItem, WrongQuantity, ExpiredOrQuality, ProcessError). The model’s **class_id** can represent the product/SKU. You map each class index to one of those defect kinds in `ClassToDefectKindMap` (e.g. “product 0 → WrongItem, product 1 → WrongItem, …”). Each `Defect` also carries **`product_id`** (the model’s class_id), so downstream (POS, alerts) knows *which* product was wrong.
- **Decoder behaviour**: `DefectDecoder` sets `Defect::product_id` from the model’s `class_id` for every detection. If `class_id` is beyond the map, `DefectKind` defaults to `ProcessError` but the raw class is still stored in `product_id`.

So: **yes, the framework can handle models with many classes.** Build a `ClassToDefectKindMap` with one entry per model class (each entry is one of the four defect kinds), and optionally use `Defect::category` or external lookup to turn `product_id` into a human-readable name. The ONNX backend does not impose a limit on the number of classes; only memory and the model’s own output size matter.

---

## Related docs

- [Inference contract](inference-contract.md) — Frame format and layout the backend receives.
- [Inference design & gaps](inference-design-and-gaps.md) — Backend interface, batching, production gaps.
- [Implementation plan](implementation_plan.md) — ONNX backend (Phase 1), TensorRT (Phase 2), config and CLI.
- [Architecture](architecture.md) — Pipeline stages and DefectDetectionStage.
