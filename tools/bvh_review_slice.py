#!/usr/bin/env python3
"""Build a deterministic BVH-to-VRMA slice from an accepted native review range."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import re

import bvh_to_vrma


SELECTION_VERSION = 1
SELECTION_KEYS = {
    "eidolon_vrma_review_selection",
    "clip_duration_milliseconds",
    "start_milliseconds",
    "end_milliseconds",
}


@dataclass(frozen=True)
class ReviewSelection:
    duration_milliseconds: int
    start_milliseconds: int
    end_milliseconds: int


def parse_selection(path: Path) -> ReviewSelection:
    fields: dict[str, int] = {}
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line:
            continue
        match = re.fullmatch(r"([a-z_]+)=([0-9]+)", line)
        if match is None:
            raise ValueError(f"selection line {line_number} is malformed")
        key, value_text = match.groups()
        if key not in SELECTION_KEYS:
            raise ValueError(f"selection line {line_number} has unknown field '{key}'")
        if key in fields:
            raise ValueError(f"selection field '{key}' is duplicated")
        fields[key] = int(value_text)
    missing = sorted(SELECTION_KEYS - fields.keys())
    if missing:
        raise ValueError(f"selection is missing field(s): {', '.join(missing)}")
    if fields["eidolon_vrma_review_selection"] != SELECTION_VERSION:
        raise ValueError("selection version is unsupported")
    duration = fields["clip_duration_milliseconds"]
    start = fields["start_milliseconds"]
    end = fields["end_milliseconds"]
    if duration < 1 or start >= end or end > duration:
        raise ValueError("selection range is empty or outside its reviewed clip")
    return ReviewSelection(duration, start, end)


def selection_frame_bounds(
    motion: bvh_to_vrma.BvhMotion,
    selection: ReviewSelection,
    base_frame: int,
) -> tuple[int, int]:
    if not 0 <= base_frame < len(motion.frames) - 1:
        raise ValueError("selection base frame is outside the BVH motion")
    frame_milliseconds = Decimal(str(motion.frame_time)) * Decimal(1000)
    expected_duration = Decimal(len(motion.frames) - 1 - base_frame) * frame_milliseconds
    reviewed_duration = Decimal(selection.duration_milliseconds)
    tolerance = max(Decimal(1), frame_milliseconds / Decimal(2))
    if abs(reviewed_duration - expected_duration) > tolerance:
        raise ValueError(
            "selection duration does not match the BVH range: "
            f"reviewed={selection.duration_milliseconds}ms "
            f"expected={expected_duration:.3f}ms"
        )

    def nearest_offset(milliseconds: int) -> int:
        return int(
            (Decimal(milliseconds) / frame_milliseconds).to_integral_value(
                rounding=ROUND_HALF_UP
            )
        )

    start_frame = base_frame + nearest_offset(selection.start_milliseconds)
    last_frame = base_frame + nearest_offset(selection.end_milliseconds)
    end_frame = last_frame + 1
    if not (base_frame <= start_frame < end_frame <= len(motion.frames)):
        raise ValueError("selection resolves outside the BVH motion")
    if end_frame - start_frame < 2:
        raise ValueError("selection must contain at least two source frames")
    return start_frame, end_frame


def convert_review_slice(
    source: Path,
    selection_path: Path,
    output: Path,
    *,
    source_uri: str,
    source_sha256: str,
    base_frame: int = 1,
    rest_frame: int = 0,
    unit_scale: float = bvh_to_vrma.DEFAULT_UNIT_SCALE,
    clip_name: str = "reviewed-slice",
    output_sha256: str | None = None,
) -> tuple[int, int]:
    motion = bvh_to_vrma.parse_bvh(source)
    selection = parse_selection(selection_path)
    start_frame, end_frame = selection_frame_bounds(motion, selection, base_frame)
    bvh_to_vrma.convert(
        source,
        output,
        source_uri=source_uri,
        source_sha256=source_sha256,
        rest_frame=rest_frame,
        start_frame=start_frame,
        end_frame=end_frame,
        unit_scale=unit_scale,
        clip_name=clip_name,
        output_sha256=output_sha256,
    )
    return start_frame, end_frame


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--selection", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--source-uri", required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--base-frame", type=int, default=1)
    parser.add_argument("--rest-frame", type=int, default=0)
    parser.add_argument("--unit-scale", type=float, default=bvh_to_vrma.DEFAULT_UNIT_SCALE)
    parser.add_argument("--clip-name", default="reviewed-slice")
    parser.add_argument("--output-sha256")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    start_frame, end_frame = convert_review_slice(
        args.source,
        args.selection,
        args.output,
        source_uri=args.source_uri,
        source_sha256=args.source_sha256.lower(),
        base_frame=args.base_frame,
        rest_frame=args.rest_frame,
        unit_scale=args.unit_scale,
        clip_name=args.clip_name,
        output_sha256=args.output_sha256,
    )
    motion = bvh_to_vrma.parse_bvh(args.source)
    print(
        f"built reviewed VRMA slice: {args.output.resolve()} "
        f"frames={start_frame}..{end_frame - 1} "
        f"duration={(end_frame - start_frame - 1) * motion.frame_time:.3f}s "
        f"sha256={bvh_to_vrma.sha256_file(args.output)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f"reviewed BVH slice conversion failed: {error}") from None
