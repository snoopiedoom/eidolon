#!/usr/bin/env python3
"""Fail closed when the EPR core acquires forbidden ownership or unbounded frame work."""

from __future__ import annotations

import pathlib
import re
import sys


FORBIDDEN = {
    r"\bSDL(?:_|[A-Z])": "SDL ownership",
    r"\bD3D11\b|\bID3D11": "D3D11 ownership",
    r"\bDirectComposition\b|\bIDComposition": "DirectComposition ownership",
    r"\bHWND\b|\bWin32\b|windows\.h": "Windows ownership",
    r"\bswapchain\b|\bswap_chain\b": "swapchain ownership",
    r'#include\s+"(?:presentation|scene|model|draw)\.h"': "renderer or presentation include",
    r"\b(?:malloc|calloc|realloc|free)\s*\(": "dynamic allocation on the bounded core path",
    r"\b(?:fopen|fread|fwrite|fprintf)\s*\(": "file I/O in the core",
    r"\bprovider\b": "legacy adapter kind or provider identity",
}


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: epr_boundary_test.py <epr-source-directory>", file=sys.stderr)
        return 2
    root = pathlib.Path(sys.argv[1])
    failures: list[str] = []
    for path in sorted(root.glob("*.[ch]")):
        text = path.read_text(encoding="utf-8")
        for pattern, reason in FORBIDDEN.items():
            for match in re.finditer(pattern, text, flags=re.IGNORECASE):
                line = text.count("\n", 0, match.start()) + 1
                failures.append(f"{path}:{line}: {reason}: {match.group(0)!r}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("epr boundary check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
