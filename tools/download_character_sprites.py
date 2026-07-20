#!/usr/bin/env python3
"""Catalog and download Blue Archive character portraits from the wiki category."""

from __future__ import annotations

import argparse
import concurrent.futures
import dataclasses
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import threading
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request


API_URL = "https://bluearchive.wiki/w/api.php"
CATEGORY = "Category:Character_sprites"
USER_AGENT = "Eidolon sprite downloader/1.0 (https://github.com/snoopiedoom/eidolon)"
FILE_PATTERN = re.compile(
    r"^(?P<character>.+?)(?: \((?P<variant>.+)\))? "
    r"(?P<expression>[0-9]+)\.(?P<extension>png|webp|jpe?g)$",
    re.IGNORECASE,
)
VARIANT_SLUG_ALIASES = {"bunny girl": "bunny"}


@dataclasses.dataclass(frozen=True)
class SpriteRecord:
    character: str
    variant: str | None
    model: str
    expression: str
    source_title: str
    source_url: str
    description_url: str
    size: int
    width: int
    height: int
    sha1: str
    timestamp: str
    relative_path: str


class RateLimiter:
    def __init__(self, interval_seconds: float) -> None:
        self.interval_seconds = interval_seconds
        self.next_request = 0.0
        self.lock = threading.Lock()

    def wait(self) -> None:
        if self.interval_seconds <= 0.0:
            return
        with self.lock:
            now = time.monotonic()
            delay = self.next_request - now
            if delay > 0.0:
                time.sleep(delay)
                now = time.monotonic()
            self.next_request = now + self.interval_seconds


def slugify(text: str) -> str:
    normalized = unicodedata.normalize("NFKD", text)
    ascii_text = normalized.encode("ascii", "ignore").decode("ascii").lower()
    return re.sub(r"[^a-z0-9]+", "-", ascii_text).strip("-")


def local_filename(source_title: str) -> str:
    underscored = source_title.replace(" ", "_")
    safe = re.sub(r'[:*?"<>|/\\]', "_", underscored)
    if safe in {"", ".", ".."}:
        raise ValueError(f"unsafe source title: {source_title!r}")
    return safe


def parse_sprite(page: dict[str, object]) -> SpriteRecord:
    title = str(page["title"])
    if title.startswith("File:"):
        title = title[5:]
    match = FILE_PATTERN.fullmatch(title)
    if match is None:
        raise ValueError(f"unrecognized sprite title: {title}")

    image_infos = page.get("imageinfo")
    if not isinstance(image_infos, list) or not image_infos:
        raise ValueError(f"missing image metadata: {title}")
    info = image_infos[0]
    if not isinstance(info, dict) or "url" not in info:
        raise ValueError(f"missing image URL: {title}")

    character = match.group("character")
    variant = match.group("variant")
    character_slug = slugify(character)
    variant_slug = ""
    if variant is not None:
        variant_slug = VARIANT_SLUG_ALIASES.get(variant.casefold(), slugify(variant))
    model = character_slug if not variant_slug else f"{character_slug}-{variant_slug}"
    filename = local_filename(title)
    relative_path = Path(model, "portraits", filename).as_posix()
    return SpriteRecord(
        character=character,
        variant=variant,
        model=model,
        expression=match.group("expression"),
        source_title=title,
        source_url=str(info["url"]),
        description_url=str(info.get("descriptionurl", "")),
        size=int(info.get("size", 0)),
        width=int(info.get("width", 0)),
        height=int(info.get("height", 0)),
        sha1=str(info.get("sha1", "")),
        timestamp=str(info.get("timestamp", "")),
        relative_path=relative_path,
    )


def request_json(parameters: dict[str, str], retries: int = 5) -> dict[str, object]:
    url = f"{API_URL}?{urllib.parse.urlencode(parameters)}"
    for attempt in range(retries):
        request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
        try:
            with urllib.request.urlopen(request, timeout=60.0) as response:
                payload = json.load(response)
            if isinstance(payload, dict) and "error" not in payload:
                return payload
            api_error = payload.get("error", payload) if isinstance(payload, dict) else payload
            if (
                isinstance(api_error, dict)
                and api_error.get("code") == "maxlag"
                and attempt + 1 < retries
            ):
                time.sleep(min(2**attempt, 16))
                continue
            raise RuntimeError(f"MediaWiki API error: {api_error}")
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError) as error:
            if attempt + 1 >= retries:
                raise RuntimeError(f"request failed after {retries} attempts: {url}") from error
            retry_after = 0.0
            if isinstance(error, urllib.error.HTTPError):
                retry_after = float(error.headers.get("Retry-After", "0") or 0)
            time.sleep(max(retry_after, min(2**attempt, 16)))
    raise AssertionError("unreachable")


def fetch_catalog() -> list[SpriteRecord]:
    base_parameters = {
        "action": "query",
        "generator": "categorymembers",
        "gcmtitle": CATEGORY,
        "gcmtype": "file",
        "gcmlimit": "max",
        "prop": "imageinfo",
        "iiprop": "url|size|sha1|timestamp|mime",
        "format": "json",
        "formatversion": "2",
        "maxlag": "5",
    }
    continuation: dict[str, str] = {}
    records: list[SpriteRecord] = []
    while True:
        payload = request_json({**base_parameters, **continuation})
        query = payload.get("query", {})
        pages = query.get("pages", []) if isinstance(query, dict) else []
        if not isinstance(pages, list):
            raise RuntimeError("MediaWiki returned an invalid page list")
        records.extend(parse_sprite(page) for page in pages if isinstance(page, dict))
        next_continuation = payload.get("continue")
        if not isinstance(next_continuation, dict):
            break
        continuation = {str(key): str(value) for key, value in next_continuation.items()}

    records.sort(key=lambda record: (record.model, int(record.expression), record.source_title))
    paths: dict[str, str] = {}
    for record in records:
        previous = paths.setdefault(record.relative_path.casefold(), record.source_title)
        if previous != record.source_title:
            raise RuntimeError(
                f"local path collision: {previous!r} and {record.source_title!r}"
            )
    return records


def matches_filter(record: SpriteRecord, characters: list[str], variants: list[str]) -> bool:
    if characters:
        choices = {record.character.casefold(), slugify(record.character), record.model}
        if not any(value.casefold() in choices for value in characters):
            return False
    if variants:
        variant_choices = {"base" if record.variant is None else record.variant.casefold()}
        if record.variant is not None:
            variant_choices.add(slugify(record.variant))
        if not any(value.casefold() in variant_choices for value in variants):
            return False
    return True


def sha1_file(path: Path) -> str:
    digest = hashlib.sha1()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def is_current(path: Path, record: SpriteRecord, verify: bool) -> bool:
    if not path.is_file() or path.stat().st_size != record.size:
        return False
    return not verify or not record.sha1 or sha1_file(path) == record.sha1


def download_one(
    output: Path, record: SpriteRecord, limiter: RateLimiter, force: bool, verify: bool
) -> str:
    destination = output / Path(record.relative_path)
    if not force and is_current(destination, record, verify=verify):
        return "skipped"

    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_name(f"{destination.name}.part")
    request = urllib.request.Request(record.source_url, headers={"User-Agent": USER_AGENT})
    try:
        for attempt in range(5):
            limiter.wait()
            try:
                digest = hashlib.sha1()
                written = 0
                with urllib.request.urlopen(request, timeout=120.0) as response, partial.open(
                    "wb"
                ) as target:
                    while True:
                        block = response.read(1024 * 1024)
                        if not block:
                            break
                        target.write(block)
                        digest.update(block)
                        written += len(block)
                if written != record.size:
                    raise RuntimeError(
                        f"size mismatch for {record.source_title}: {written} != {record.size}"
                    )
                if record.sha1 and digest.hexdigest() != record.sha1:
                    raise RuntimeError(f"SHA-1 mismatch for {record.source_title}")
                os.replace(partial, destination)
                return "downloaded"
            except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError, RuntimeError):
                partial.unlink(missing_ok=True)
                if attempt == 4:
                    raise
                time.sleep(min(2**attempt, 16))
    finally:
        partial.unlink(missing_ok=True)
    raise AssertionError("unreachable")


def human_size(size: int) -> str:
    value = float(size)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if value < 1024.0 or unit == "TiB":
            return f"{value:.1f} {unit}"
        value /= 1024.0
    raise AssertionError("unreachable")


def write_manifest(path: Path, records: list[SpriteRecord]) -> None:
    characters = {record.character for record in records}
    models = {record.model for record in records}
    document = {
        "version": 1,
        "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "source": {
            "category": CATEGORY,
            "category_url": "https://bluearchive.wiki/wiki/Category:Character_sprites",
            "api_url": API_URL,
        },
        "summary": {
            "characters": len(characters),
            "models": len(models),
            "files": len(records),
            "bytes": sum(record.size for record in records),
        },
        "sprites": [dataclasses.asdict(record) for record in records],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(f"{path.suffix}.part")
    temporary.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Catalog Blue Archive character sprites. Downloading is opt-in because the complete "
            "category contains thousands of large PNG files."
        )
    )
    parser.add_argument("--output", type=Path, default=Path("assets/characters"))
    parser.add_argument("--download", action="store_true", help="download the selected files")
    parser.add_argument(
        "--manifest-only", action="store_true", help="write the catalog manifest without downloads"
    )
    parser.add_argument("--character", action="append", default=[], help="character name or slug")
    parser.add_argument(
        "--variant", action="append", default=[], help="variant name/slug, or 'base'"
    )
    parser.add_argument("--limit", type=int, default=0, help="limit selected files for testing")
    parser.add_argument("--jobs", type=int, default=4, help="parallel downloads (default: 4)")
    parser.add_argument(
        "--delay", type=float, default=0.10, help="minimum delay between HTTP starts"
    )
    parser.add_argument("--force", action="store_true", help="redownload matching files")
    parser.add_argument(
        "--verify", action="store_true", help="SHA-1 check existing files while planning"
    )
    parser.add_argument("--quiet", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    if arguments.jobs < 1 or arguments.jobs > 32:
        raise SystemExit("--jobs must be between 1 and 32")
    if arguments.delay < 0.0 or arguments.limit < 0:
        raise SystemExit("--delay and --limit cannot be negative")

    records = [
        record
        for record in fetch_catalog()
        if matches_filter(record, arguments.character, arguments.variant)
    ]
    if arguments.limit:
        records = records[: arguments.limit]
    if not records:
        raise SystemExit("no sprites matched the requested filters")

    output = arguments.output.resolve()
    planned = [
        record
        for record in records
        if arguments.force
        or not is_current(output / Path(record.relative_path), record, arguments.verify)
    ]
    total_bytes = sum(record.size for record in records)
    pending_bytes = sum(record.size for record in planned)
    models = {record.model for record in records}
    print(
        f"catalog: {len(records)} files, {len(models)} character/variants, "
        f"{human_size(total_bytes)}"
    )
    print(f"pending: {len(planned)} files, {human_size(pending_bytes)} -> {output}")

    if not arguments.download and not arguments.manifest_only:
        print("dry run only; add --download or --manifest-only")
        return 0

    if arguments.download and planned:
        output.mkdir(parents=True, exist_ok=True)
        free_bytes = shutil.disk_usage(output).free
        if free_bytes < int(pending_bytes * 1.10):
            raise SystemExit(
                f"not enough free space: need about {human_size(int(pending_bytes * 1.10))}, "
                f"have {human_size(free_bytes)}"
            )
        limiter = RateLimiter(arguments.delay)
        completed = 0
        downloaded = 0
        skipped = 0
        with concurrent.futures.ThreadPoolExecutor(max_workers=arguments.jobs) as executor:
            futures = {
                executor.submit(
                    download_one,
                    output,
                    record,
                    limiter,
                    arguments.force,
                    arguments.verify,
                ): record
                for record in records
            }
            for future in concurrent.futures.as_completed(futures):
                record = futures[future]
                try:
                    result = future.result()
                except Exception as error:
                    raise RuntimeError(f"download failed: {record.source_title}") from error
                completed += 1
                downloaded += result == "downloaded"
                skipped += result == "skipped"
                if not arguments.quiet and (completed % 50 == 0 or completed == len(records)):
                    print(
                        f"progress: {completed}/{len(records)} "
                        f"downloaded={downloaded} skipped={skipped}"
                    )

    manifest = output / "sprites-manifest.json"
    write_manifest(manifest, records)
    print(f"manifest: {manifest}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        raise SystemExit(130)
