from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import bvh_review_slice  # noqa: E402
import bvh_to_vrma  # noqa: E402
from bvh_to_vrma_test import fixture_bvh  # noqa: E402


class BvhReviewSliceTest(unittest.TestCase):
    def write_selection(self, path: Path, duration: int, start: int, end: int) -> None:
        path.write_text(
            "eidolon_vrma_review_selection=1\n"
            f"clip_duration_milliseconds={duration}\n"
            f"start_milliseconds={start}\n"
            f"end_milliseconds={end}\n",
            encoding="utf-8",
        )

    def test_selection_resolves_to_inclusive_review_marks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "motion.bvh"
            selection_path = root / "selection.txt"
            output = root / "slice.vrma"
            source.write_text(fixture_bvh(), encoding="utf-8")
            self.write_selection(selection_path, duration=20, start=0, end=10)
            source_digest = hashlib.sha256(source.read_bytes()).hexdigest()

            bounds = bvh_review_slice.convert_review_slice(
                source,
                selection_path,
                output,
                source_uri="https://example.invalid/pinned/motion.bvh",
                source_sha256=source_digest,
                base_frame=0,
                clip_name="reviewed-test",
            )
            self.assertEqual(bounds, (0, 2))
            payload = output.read_bytes()
            json_length, json_type = struct.unpack_from("<II", payload, 12)
            self.assertEqual(json_type, bvh_to_vrma.GLB_JSON_CHUNK)
            document = json.loads(payload[20 : 20 + json_length].decode("utf-8"))
            evidence = document["extras"]["eidolonFixture"]
            self.assertEqual(evidence["startFrame"], 0)
            self.assertEqual(evidence["endFrameExclusive"], 2)

    def test_selection_rejects_duration_mismatch_and_duplicate_fields(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "motion.bvh"
            selection_path = root / "selection.txt"
            source.write_text(fixture_bvh(), encoding="utf-8")
            motion = bvh_to_vrma.parse_bvh(source)

            self.write_selection(selection_path, duration=200, start=0, end=10)
            selection = bvh_review_slice.parse_selection(selection_path)
            with self.assertRaisesRegex(ValueError, "duration does not match"):
                bvh_review_slice.selection_frame_bounds(motion, selection, 0)

            selection_path.write_text(
                "eidolon_vrma_review_selection=1\n"
                "clip_duration_milliseconds=20\n"
                "start_milliseconds=0\n"
                "start_milliseconds=1\n"
                "end_milliseconds=10\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicated"):
                bvh_review_slice.parse_selection(selection_path)


if __name__ == "__main__":
    unittest.main()
