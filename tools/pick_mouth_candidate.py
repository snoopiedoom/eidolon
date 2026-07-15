"""Promote one reviewed mouth candidate to the canonical texture."""

import argparse
import json
import shutil
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--pick", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    candidates = json.loads(args.manifest.read_text(encoding="utf-8"))
    selected = next((item for item in candidates if item["label"] == args.pick.upper()), None)
    if selected is None:
        raise RuntimeError(f"unknown mouth candidate: {args.pick}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(selected["texture"], args.output)
    print(
        f"selected {selected['label']}: x={selected['offset_x']} y={selected['offset_y']} "
        f"-> {args.output}"
    )


if __name__ == "__main__":
    main()
