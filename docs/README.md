# Project Documentation

Documentation for this C++23 Computer Vision / Edge AI example project (defect detection pipeline).

## Project Aim

**Detect defects in shopping items** — The system analyzes images of retail products (e.g. at checkout, on shelves, or in back-of-store) and identifies defects such as:

- **Damage** — Dented cans, torn packaging, crushed boxes, broken seals
- **Quality issues** — Bruised produce, expired or damaged labels, wrong/missing items
- **Anomalies** — Items that don’t match expected product (e.g. wrong barcode vs visual)

The pipeline is designed for **edge deployment** (e.g. at POS or in-store) so defect detection runs locally with low latency. This is an **example project**; no third-party company names or brands are used with permission or endorsement.

**Docs and naming:** We take care that **documentation and all .md files** present the project as an independent example and do not advertise or brand it under any third-party name. **Internal code** (namespaces, script names, CMake targets, e.g. `normitri::`, `normitri-cli`, `build_nomitri.sh`) may keep their current names for consistency; they are not used for public advertising.

## Contents

| Document | Description |
|----------|-------------|
| [**Pipeline workflow**](workflow.md) | Detailed workflow diagrams (Mermaid). The main pipeline sketch is in the [root README](../README.md#pipeline-workflow). |
| [Company context & aim](company-and-aim.md) | Example domain context and project aim (defect detection in shopping items) |
| [Architecture](architecture.md) | High-level design, modules, and data flow |
| [Building](building.md) | CMake, Conan, and build options |
| [Development](development.md) | Coding standards, module contents (core/vision/app), testing, and CI |
| [Local CI testing (act)](act-local-testing.md) | Run GitHub Actions workflows locally with `act` before pushing |
| [Detection efficiency](detection-efficiency.md) | Research summary: detection by item type/category/price; design implications before code |
| [API Reference](api-reference.md) | Core types and pipeline interfaces |

## Project Aim

The project implements a **defect-detection pipeline for shopping items** (wrong item, wrong quantity, quality/expiry-related issues) as an **example** for retail/edge use cases. See [Company context & aim](company-and-aim.md) for example domain context.

## Project Overview

This project demonstrates skills relevant to **Edge AI and Computer Vision** roles:

- **C++23** — Modern language features, threading, and memory management
- **Computer Vision** — Image processing pipeline and extensible inference hooks
- **Build & DevOps** — CMake, optional Conan, CI/CD, and containerization
- **Quality** — Unit tests, static analysis, and clear architecture

## Quick Links

- [**Pipeline workflow**](../README.md#pipeline-workflow) — One diagram in root README; [more diagrams](workflow.md) here
- [Getting started](building.md#getting-started) — Build and run
- [Adding a pipeline stage](architecture.md#pipeline-stages) — Extend the CV pipeline
- [Running tests](development.md#testing) — Test suite and coverage
- [Local CI with act](act-local-testing.md) — Test workflows locally before pushing
- [Detection efficiency & product focus](detection-efficiency.md) — Expensive vs cheap, categories, what to design for before code
