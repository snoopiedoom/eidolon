"""Compose rendered mouth candidates into one labeled review sheet."""

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--renders", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    candidates = json.loads(args.manifest.read_text(encoding="utf-8"))
    cell = 256
    sheet = Image.new("RGB", (cell * 5, cell * 5), (12, 12, 14))
    draw = ImageDraw.Draw(sheet)
    for index, candidate in enumerate(candidates):
        row, column = divmod(index, 5)
        image = Image.open(args.renders / f"{candidate['label']}.png").convert("RGBA")
        background = Image.new("RGBA", image.size, (12, 12, 14, 255))
        background.alpha_composite(image)
        sheet.paste(background.convert("RGB"), (column * cell, row * cell))
        origin = (column * cell + 8, row * cell + 8)
        draw.rectangle((origin[0] - 3, origin[1] - 3, origin[0] + 27, origin[1] + 13), fill=(0, 0, 0))
        draw.text(origin, candidate["label"], fill=(255, 255, 255))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output, optimize=True)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
