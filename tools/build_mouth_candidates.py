"""Build a labeled position grid for visual mouth calibration."""

import argparse
import json
from pathlib import Path

from build_mouth_texture import build_texture


OFFSETS = (-16, -8, 0, 8, 16)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    candidates = []
    for row, offset_y in enumerate(OFFSETS):
        for column, offset_x in enumerate(OFFSETS):
            label = f"{chr(ord('A') + row)}{column + 1}"
            texture = args.output / f"{label}.png"
            build_texture(args.input, texture, offset_x=offset_x, offset_y=offset_y)
            candidates.append({
                "label": label,
                "offset_x": offset_x,
                "offset_y": offset_y,
                "texture": str(texture.resolve()),
            })
    (args.output / "manifest.json").write_text(
        json.dumps(candidates, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
