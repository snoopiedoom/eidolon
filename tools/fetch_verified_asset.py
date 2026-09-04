#!/usr/bin/env python3
"""Download one explicitly requested asset and install it only after SHA-256 verification."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import tempfile
import urllib.error
import urllib.parse
import urllib.request


USER_AGENT = "Eidolon verified fixture fetcher/1"
CHUNK_SIZE = 1024 * 1024


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(CHUNK_SIZE):
            digest.update(chunk)
    return digest.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    expected = args.sha256.lower()
    parsed_url = urllib.parse.urlparse(args.url)
    if parsed_url.scheme != "https" or len(expected) != 64 or any(
        character not in "0123456789abcdef" for character in expected
    ):
        raise SystemExit("verified asset input requires an HTTPS URL and 64-digit SHA-256")

    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.is_file() and sha256_file(output) == expected:
        print(f"verified asset ready: {output}")
        return 0

    request = urllib.request.Request(args.url, headers={"User-Agent": USER_AGENT})
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=f".{output.name}.", suffix=".partial", dir=output.parent,
            delete=False
        ) as temporary:
            temporary_path = Path(temporary.name)
            digest = hashlib.sha256()
            with urllib.request.urlopen(request, timeout=60.0) as response:
                if urllib.parse.urlparse(response.geturl()).scheme != "https":
                    raise RuntimeError("verified asset download redirected away from HTTPS")
                while chunk := response.read(CHUNK_SIZE):
                    temporary.write(chunk)
                    digest.update(chunk)
            temporary.flush()
            os.fsync(temporary.fileno())
        actual = digest.hexdigest()
        if actual != expected:
            raise RuntimeError(
                f"SHA-256 mismatch for {args.url}: expected {expected}, received {actual}"
            )
        os.replace(temporary_path, output)
        temporary_path = None
        print(f"downloaded verified asset: {output}")
        return 0
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, urllib.error.URLError) as error:
        raise SystemExit(f"verified asset fetch failed: {error}") from None
