# Example Domain Context & Project Aim

**Disclaimer:** This project is an **independent example**. No company names, brands, or products are used with permission or endorsement. The domain context below describes the *type* of use case (retail, edge AI, defect detection) for illustration only.

---

## Example Domain: Retail Edge AI

Many retailers use **on-device AI** for:

- **Edge deployment** — Run vision models at the store (POS, camera, smart cart) with low latency and minimal cloud dependency.
- **Defect and error detection** — Wrong item, wrong quantity, quality/expiry, process errors (e.g. not scanned, barcode switch).
- **Use cases** — Self-checkout (SCO) fraud and produce recognition, pick & pack (wrong item/quantity, expired), smart cart (mistakes).

Typical technical goals in this space: &lt;30 ms per image, inference &lt;10 ms, support for object detection, classification, and various hardware backends.

---

## What “Defect in Shopping Items” Means (Example Scope)

- **Wrong item** — e.g. wrong product in basket or in picked order.
- **Wrong quantity** — too many or too few.
- **Expired or unsuitable products** — e.g. in pick & pack.
- **Process errors** — e.g. not scanned, barcode switched, non-compliance.

This **example project** implements a **C++23 edge-style pipeline** that detects such defects (wrong item, wrong quantity, quality/expiry-related issues) as a demonstration for similar retail/edge use cases.

---

## Project Aim

**Primary aim:** Design and implement a modular **defect-detection pipeline for shopping items** that:

1. **Ingests** images (or video frames) of items at checkout, in pick stations, or in cart.
2. **Preprocesses** frames (resize, normalize, format) for a vision/inference backend.
3. **Runs inference** (or a mock) to detect **defects** such as:
   - Wrong item (e.g. produce recognition / product ID mismatch).
   - Wrong quantity (count or region-based).
   - Quality/expiry cues (optional; can be placeholder).
4. **Outputs** structured results (e.g. defect type, location, confidence) for alerts, POS integration, or supervisor dashboards.

**Secondary aims:**

- Demonstrate **modern C++23**, **CMake**, **threading**, and **clear separation** between core, vision, and app.
- Stay **backend-agnostic** (plug-in ONNX, TensorRT, etc.) and **edge-friendly** (low latency, minimal dependencies).
- Suited to **example** retail/edge deployment: POS integration, scalability, configurable pipeline.

This is documented in [Architecture](architecture.md) and [API Reference](api-reference.md); the contents of **core**, **vision**, and **app** are specified there and in [Development](development.md).
