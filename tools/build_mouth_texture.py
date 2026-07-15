"""Fit a mouth-only alpha source into Rio's existing mouth-plane UV rectangle."""

import argparse
from pathlib import Path

from PIL import Image


UV_MIN = (0.1122414842, 0.5185304880)
UV_MAX = (0.2626113296, 0.6252534389)
ATLAS_SIZE = 1024
MOUTH_WIDTH = 40
MOUTH_HEIGHT = 3


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--offset-x", type=int, default=0)
    parser.add_argument("--offset-y", type=int, default=0)
    parser.add_argument("--width", type=int, default=MOUTH_WIDTH)
    parser.add_argument("--height", type=int, default=MOUTH_HEIGHT)
    return parser.parse_args()


def alpha_bounds(image):
    bounds = image.getchannel("A").getbbox()
    if bounds is None:
        raise RuntimeError("mouth source contains no opaque pixels")
    return bounds


def build_texture(input_path, output_path, offset_x=0, offset_y=0, width=MOUTH_WIDTH,
                  height=MOUTH_HEIGHT):
    source = Image.open(input_path).convert("RGBA")
    source = source.crop(alpha_bounds(source))
    source = source.resize((width, height), Image.Resampling.LANCZOS)

    # The source hue is irrelevant: Character_Mouth_Black is an ink/alpha mask.
    alpha = source.getchannel("A")
    ink = Image.new("RGBA", source.size, (0, 0, 0, 0))
    ink.putalpha(alpha)

    center_u = (UV_MIN[0] + UV_MAX[0]) * 0.5
    center_v = (UV_MIN[1] + UV_MAX[1]) * 0.5
    center_x = round(center_u * ATLAS_SIZE) + offset_x
    center_y = round((1.0 - center_v) * ATLAS_SIZE) + offset_y
    position = (center_x - ink.width // 2, center_y - ink.height // 2)

    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE), (0, 0, 0, 0))
    atlas.alpha_composite(ink, position)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output_path, optimize=True)
    print(f"wrote {output_path} at {position} size={ink.size}")


def main():
    args = parse_arguments()
    build_texture(args.input, args.output, args.offset_x, args.offset_y, args.width, args.height)


if __name__ == "__main__":
    main()
