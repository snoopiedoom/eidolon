#!/usr/bin/env python3
"""Generate a clang compilation database from Eidolon's real Make recipes."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys


COMPILE_SOURCE = re.compile(r'(?:^|\s)-c\s+(?:"([^"]+)"|(\S+))(?=\s)')


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--make", default="make")
    parser.add_argument("--mode", default="debug", choices=("debug", "release"))
    parser.add_argument("--target", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    root = pathlib.Path.cwd().resolve()
    result = subprocess.run(
        [args.make, "-Bn", f"MODE={args.mode}", args.target],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        return result.returncode

    entries: list[dict[str, str]] = []
    for raw_line in result.stdout.splitlines():
        command = raw_line.strip()
        match = COMPILE_SOURCE.search(command)
        if match is None:
            continue
        source = match.group(1) or match.group(2)
        source_path = pathlib.Path(source)
        if not source_path.is_absolute():
            source_path = root / source_path
        entries.append(
            {
                "directory": str(root),
                "command": command,
                "file": str(source_path.resolve()),
            }
        )

    if not entries:
        sys.stderr.write("no compiler invocations found in Make dry run\n")
        return 1

    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    temporary.replace(output)
    print(f"wrote {len(entries)} translation units to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
