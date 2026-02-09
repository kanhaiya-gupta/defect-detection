# Flowchart images

Mermaid source (`.mmd`) and generated PNGs for pipeline workflow diagrams.

## Source files

| File | Description |
|------|-------------|
| `pipeline_overview.mmd` | Simple linear pipeline (README overview). |
| `single_frame_pipeline.mmd` | Single-frame flow with subgraphs (preprocess, inference, output). |
| `end_to_end_flow.mmd` | End-to-end flow with layers and class styling. |
| `batch_parallel.mmd` | Batch and parallel execution modes. |

## Generate PNGs

Requires [Mermaid CLI](https://github.com/mermaid-js/mermaid-cli):

```bash
npm install -g @mermaid-js/mermaid-cli
```

From the repo root:

```bash
# Generate all PNGs (output next to each .mmd)
python scripts/convert_mermaid_to_png.py docs/flowchart_images/ --all

# Single file, custom scale
python scripts/convert_mermaid_to_png.py docs/flowchart_images/pipeline_overview.mmd --scale 6
```

PNGs are written to this directory (e.g. `pipeline_overview.png`). Use them in docs, slides, or the main README if you prefer embedded images over inline Mermaid.
