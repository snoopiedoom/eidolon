from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

from tools import bvh_to_vrma


PARENTS = {
    "Hips": None,
    "LHipJoint": "Hips",
    "LeftUpLeg": "LHipJoint",
    "LeftLeg": "LeftUpLeg",
    "LeftFoot": "LeftLeg",
    "LeftToeBase": "LeftFoot",
    "RHipJoint": "Hips",
    "RightUpLeg": "RHipJoint",
    "RightLeg": "RightUpLeg",
    "RightFoot": "RightLeg",
    "RightToeBase": "RightFoot",
    "LowerBack": "Hips",
    "Spine": "LowerBack",
    "Spine1": "Spine",
    "Neck": "Spine1",
    "Neck1": "Neck",
    "Head": "Neck1",
    "LeftShoulder": "Spine1",
    "LeftArm": "LeftShoulder",
    "LeftForeArm": "LeftArm",
    "LeftHand": "LeftForeArm",
    "RightShoulder": "Spine1",
    "RightArm": "RightShoulder",
    "RightForeArm": "RightArm",
    "RightHand": "RightForeArm",
}


def fixture_bvh() -> str:
    children: dict[str, list[str]] = {name: [] for name in PARENTS}
    for name, parent in PARENTS.items():
        if parent is not None:
            children[parent].append(name)
    channel_count = 0

    def emit(name: str, depth: int) -> list[str]:
        nonlocal channel_count
        indent = "\t" * depth
        lines = [f"{indent}{'ROOT' if PARENTS[name] is None else 'JOINT'} {name}",
                 f"{indent}{{"]
        if name == "Hips":
            offset = "0 0 0"
            channels = "CHANNELS 6 Xposition Yposition Zposition Zrotation Yrotation Xrotation"
            channel_count += 6
        else:
            if "Left" in name:
                offset = "1 1 0"
            elif "Right" in name:
                offset = "-1 1 0"
            else:
                offset = "0 1 0"
            channels = "CHANNELS 3 Zrotation Yrotation Xrotation"
            channel_count += 3
        lines.extend((f"{indent}\tOFFSET {offset}", f"{indent}\t{channels}"))
        for child in children[name]:
            lines.extend(emit(child, depth + 1))
        lines.append(f"{indent}}}")
        return lines

    hierarchy = ["HIERARCHY", *emit("Hips", 0)]
    frames = []
    for frame in range(3):
        values = [0.0] * channel_count
        values[1] = 40.0
        if frame > 0:
            values[3] = float(frame * 10)
            values[6] = float(frame * 5)
        frames.append(" ".join(str(value) for value in values))
    return "\n".join((*hierarchy, "MOTION", "Frames: 3", "Frame Time: .01", *frames, ""))


class BvhToVrmaTest(unittest.TestCase):
    def test_parser_and_world_evaluation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "motion.bvh"
            path.write_text(fixture_bvh(), encoding="utf-8")
            motion = bvh_to_vrma.parse_bvh(path)
            self.assertEqual(len(motion.frames), 3)
            self.assertEqual(motion.channel_count, 6 + 3 * (len(PARENTS) - 1))
            positions, rotations = bvh_to_vrma.evaluate_world(motion, motion.frames[0])
            self.assertAlmostEqual(positions[0][1], 40.0)
            self.assertAlmostEqual(sum(value * value for value in rotations[0]), 1.0)

    def test_conversion_is_deterministic_and_declares_humanoid(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "motion.bvh"
            first = Path(directory) / "first.vrma"
            second = Path(directory) / "second.vrma"
            source.write_text(fixture_bvh(), encoding="utf-8")
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            arguments = {
                "source_uri": "https://example.invalid/pinned/motion.bvh",
                "source_sha256": digest,
                "clip_name": "test-walk",
            }
            bvh_to_vrma.convert(source, first, **arguments)
            payload = first.read_bytes()
            output_digest = hashlib.sha256(payload).hexdigest()
            bvh_to_vrma.convert(source, second, output_sha256=output_digest, **arguments)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            magic, version, total = struct.unpack_from("<III", payload, 0)
            self.assertEqual((magic, version, total),
                             (bvh_to_vrma.GLB_MAGIC, 2, len(payload)))
            json_length, json_type = struct.unpack_from("<II", payload, 12)
            self.assertEqual(json_type, bvh_to_vrma.GLB_JSON_CHUNK)
            document = json.loads(payload[20:20 + json_length].decode("utf-8"))
            extension = document["extensions"]["VRMC_vrm_animation"]
            self.assertEqual(extension["specVersion"], "1.0")
            bones = extension["humanoid"]["humanBones"]
            self.assertEqual(len(bones), len(bvh_to_vrma.HUMANOID_JOINTS))
            self.assertIn("hips", bones)
            self.assertIn("leftFoot", bones)
            self.assertEqual(len(document["animations"][0]["channels"]),
                             len(bvh_to_vrma.HUMANOID_JOINTS) + 1)


if __name__ == "__main__":
    unittest.main()
