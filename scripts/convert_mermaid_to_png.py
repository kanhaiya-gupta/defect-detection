#!/usr/bin/env python3
"""
Convert Mermaid (.mmd) files to PNG images.

Requirements:
    npm install -g @mermaid-js/mermaid-cli

Usage:
    # Convert all flowcharts in docs/flowchart_images/
    python scripts/convert_mermaid_to_png.py docs/flowchart_images/ --all

    # Convert a specific file
    python scripts/convert_mermaid_to_png.py docs/flowchart_images/pipeline_overview.mmd

    # Custom scale or background
    python scripts/convert_mermaid_to_png.py docs/flowchart_images/ --all --scale 4 --background white
"""

import argparse
import subprocess
import sys
from pathlib import Path
from typing import Optional


def mermaid_to_png(
    input_file: Path,
    output_file: Path,
    scale: int = 4,
    background: str = "white",
) -> bool:
    """Convert Mermaid file to PNG using mmdc."""
    try:
        subprocess.run(
            [
                "mmdc",
                "-i", str(input_file),
                "-o", str(output_file),
                "-s", str(scale),
                "--backgroundColor", background,
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        print(f"  ✓ {input_file.name} → {output_file.name}")
        return True
    except subprocess.CalledProcessError as e:
        error_msg = e.stderr if e.stderr else str(e)
        print(f"  ✗ {input_file.name}: {error_msg}")
        return False
    except FileNotFoundError:
        print("  ✗ 'mmdc' not found. Install: npm install -g @mermaid-js/mermaid-cli")
        return False


def check_mermaid_cli() -> bool:
    """Check if Mermaid CLI is available."""
    try:
        result = subprocess.run(
            ["mmdc", "--version"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        return result.returncode == 0
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False


def convert_file(
    mmd_file: Path,
    scale: int = 4,
    background: str = "white",
    output_dir: Optional[Path] = None,
) -> bool:
    """Convert a single .mmd file to PNG."""
    if not mmd_file.exists():
        print(f"  ✗ Not found: {mmd_file}")
        return False
    if mmd_file.suffix != ".mmd":
        print(f"  ✗ Not a .mmd file: {mmd_file}")
        return False

    if output_dir is not None:
        output_dir.mkdir(parents=True, exist_ok=True)
        png_file = output_dir / f"{mmd_file.stem}.png"
    else:
        png_file = mmd_file.parent / f"{mmd_file.stem}.png"

    return mermaid_to_png(mmd_file, png_file, scale=scale, background=background)


def convert_directory(
    directory: Path,
    scale: int = 4,
    background: str = "white",
    recursive: bool = False,
) -> bool:
    """Convert all .mmd files in a directory."""
    if not directory.exists() or not directory.is_dir():
        print(f"  ✗ Directory not found: {directory}")
        return False

    if recursive:
        mmd_files = sorted(directory.rglob("*.mmd"))
    else:
        mmd_files = sorted(directory.glob("*.mmd"))

    if not mmd_files:
        print(f"  No .mmd files in {directory}")
        return False

    print(f"  Found {len(mmd_files)} .mmd file(s)")
    success = 0
    for mmd_file in mmd_files:
        if convert_file(mmd_file, scale=scale, background=background):
            success += 1
    return success == len(mmd_files)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert Mermaid (.mmd) files to PNG images",
    )
    parser.add_argument(
        "input",
        type=str,
        help="Input .mmd file or directory (relative to repo root)",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Convert all .mmd files in the given directory",
    )
    parser.add_argument(
        "--scale",
        type=int,
        default=4,
        help="Scale factor for PNG (default: 4)",
    )
    parser.add_argument(
        "--background",
        type=str,
        default="white",
        help="Background color (default: white)",
    )
    parser.add_argument(
        "--recursive",
        action="store_true",
        help="Recurse into subdirectories (with --all)",
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default=None,
        help="Output directory (default: same as input)",
    )
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent
    input_path = (repo_root / args.input).resolve()
    output_dir = (repo_root / args.output_dir).resolve() if args.output_dir else None

    print("Mermaid → PNG")
    print(f"  Input: {input_path}")

    if not check_mermaid_cli():
        print("  Install: npm install -g @mermaid-js/mermaid-cli")
        print("  See: https://github.com/mermaid-js/mermaid-cli")
        sys.exit(1)

    if args.all or input_path.is_dir():
        ok = convert_directory(
            input_path,
            scale=args.scale,
            background=args.background,
            recursive=args.recursive,
        )
    else:
        ok = convert_file(
            input_path,
            scale=args.scale,
            background=args.background,
            output_dir=output_dir,
        )

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
