#!/usr/bin/env python3
"""Compile a T-pose-prefixed BVH motion into a deterministic VRM Animation GLB."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import tempfile
from typing import Iterable


GLB_MAGIC = 0x46546C67
GLB_VERSION = 2
GLB_JSON_CHUNK = 0x4E4F534A
GLB_BIN_CHUNK = 0x004E4942
FLOAT_COMPONENT = 5126
DEFAULT_UNIT_SCALE = 0.0254

Vec3 = tuple[float, float, float]
Quat = tuple[float, float, float, float]


@dataclass
class Joint:
    name: str
    parent: int | None
    offset: Vec3 = (0.0, 0.0, 0.0)
    channels: tuple[str, ...] = ()
    channel_offset: int = 0


@dataclass
class BvhMotion:
    joints: list[Joint]
    frames: list[tuple[float, ...]]
    frame_time: float
    channel_count: int


HUMANOID_JOINTS: tuple[tuple[str, str], ...] = (
    ("hips", "Hips"),
    ("spine", "LowerBack"),
    ("chest", "Spine"),
    ("upperChest", "Spine1"),
    ("neck", "Neck"),
    ("head", "Head"),
    ("leftShoulder", "LeftShoulder"),
    ("leftUpperArm", "LeftArm"),
    ("leftLowerArm", "LeftForeArm"),
    ("leftHand", "LeftHand"),
    ("rightShoulder", "RightShoulder"),
    ("rightUpperArm", "RightArm"),
    ("rightLowerArm", "RightForeArm"),
    ("rightHand", "RightHand"),
    ("leftUpperLeg", "LeftUpLeg"),
    ("leftLowerLeg", "LeftLeg"),
    ("leftFoot", "LeftFoot"),
    ("leftToes", "LeftToeBase"),
    ("rightUpperLeg", "RightUpLeg"),
    ("rightLowerLeg", "RightLeg"),
    ("rightFoot", "RightFoot"),
    ("rightToes", "RightToeBase"),
)


class HierarchyParser:
    def __init__(self, text: str) -> None:
        self.tokens = re.findall(r"[{}]|[^\s{}]+", text)
        self.position = 0
        self.joints: list[Joint] = []
        self.channel_count = 0

    def take(self, expected: str | None = None) -> str:
        if self.position >= len(self.tokens):
            raise ValueError("unexpected end of BVH hierarchy")
        token = self.tokens[self.position]
        self.position += 1
        if expected is not None and token != expected:
            raise ValueError(f"expected '{expected}' in BVH hierarchy, found '{token}'")
        return token

    def number(self) -> float:
        token = self.take()
        try:
            value = float(token)
        except ValueError as error:
            raise ValueError(f"invalid BVH number '{token}'") from error
        if not math.isfinite(value):
            raise ValueError("BVH hierarchy contains a non-finite number")
        return value

    def parse_end_site(self) -> None:
        self.take("Site")
        self.take("{")
        self.take("OFFSET")
        self.number()
        self.number()
        self.number()
        self.take("}")

    def parse_joint(self, name: str, parent: int | None) -> int:
        index = len(self.joints)
        self.joints.append(Joint(name=name, parent=parent))
        self.take("{")
        saw_offset = False
        saw_channels = False
        while True:
            token = self.take()
            if token == "}":
                break
            if token == "OFFSET":
                if saw_offset:
                    raise ValueError(f"BVH joint '{name}' repeats OFFSET")
                self.joints[index].offset = (self.number(), self.number(), self.number())
                saw_offset = True
            elif token == "CHANNELS":
                if saw_channels:
                    raise ValueError(f"BVH joint '{name}' repeats CHANNELS")
                count = int(self.take())
                if count < 0:
                    raise ValueError(f"BVH joint '{name}' has a negative channel count")
                channels = tuple(self.take() for _ in range(count))
                supported = {
                    "Xposition", "Yposition", "Zposition",
                    "Xrotation", "Yrotation", "Zrotation",
                }
                if any(channel not in supported for channel in channels):
                    raise ValueError(f"BVH joint '{name}' has an unsupported channel")
                self.joints[index].channels = channels
                self.joints[index].channel_offset = self.channel_count
                self.channel_count += count
                saw_channels = True
            elif token == "JOINT":
                self.parse_joint(self.take(), index)
            elif token == "End":
                self.parse_end_site()
            else:
                raise ValueError(f"unexpected BVH hierarchy token '{token}'")
        if not saw_offset or not saw_channels:
            raise ValueError(f"BVH joint '{name}' is incomplete")
        return index

    def parse(self) -> tuple[list[Joint], int]:
        self.take("HIERARCHY")
        self.take("ROOT")
        self.parse_joint(self.take(), None)
        if self.position != len(self.tokens):
            raise ValueError("BVH hierarchy has trailing tokens")
        names = [joint.name for joint in self.joints]
        if len(names) != len(set(names)):
            raise ValueError("BVH hierarchy has duplicate joint names")
        return self.joints, self.channel_count


def parse_bvh(path: Path) -> BvhMotion:
    text = path.read_text(encoding="utf-8-sig")
    marker = re.search(r"(?m)^\s*MOTION\s*$", text)
    if marker is None:
        raise ValueError("BVH MOTION section is missing")
    joints, channel_count = HierarchyParser(text[: marker.start()]).parse()
    lines = [line.strip() for line in text[marker.end() :].splitlines() if line.strip()]
    if len(lines) < 3:
        raise ValueError("BVH MOTION section is incomplete")
    frame_match = re.fullmatch(r"Frames:\s*(\d+)", lines[0], flags=re.IGNORECASE)
    time_match = re.fullmatch(
        r"Frame\s+Time:\s*([+\-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+\-]?\d+)?)",
        lines[1], flags=re.IGNORECASE,
    )
    if frame_match is None or time_match is None:
        raise ValueError("BVH frame count or frame time is invalid")
    frame_count = int(frame_match.group(1))
    frame_time = float(time_match.group(1))
    if frame_count < 1 or not math.isfinite(frame_time) or frame_time <= 0.0:
        raise ValueError("BVH motion timing must be positive and finite")
    try:
        values = tuple(float(token) for line in lines[2:] for token in line.split())
    except ValueError as error:
        raise ValueError("BVH motion contains a non-numeric sample") from error
    expected = frame_count * channel_count
    if len(values) != expected or any(not math.isfinite(value) for value in values):
        raise ValueError(
            f"BVH motion sample count mismatch: expected {expected}, received {len(values)}"
        )
    frames = [values[index : index + channel_count]
              for index in range(0, len(values), channel_count)]
    return BvhMotion(joints=joints, frames=frames, frame_time=frame_time,
                     channel_count=channel_count)


def quat_normalize(value: Quat) -> Quat:
    length = math.sqrt(sum(component * component for component in value))
    if not math.isfinite(length) or length <= 1.0e-12:
        raise ValueError("motion produced an invalid quaternion")
    return tuple(component / length for component in value)  # type: ignore[return-value]


def quat_multiply(left: Quat, right: Quat) -> Quat:
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return (
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    )


def quat_inverse(value: Quat) -> Quat:
    x, y, z, w = quat_normalize(value)
    return (-x, -y, -z, w)


def axis_quaternion(channel: str, degrees: float) -> Quat:
    radians = math.radians(degrees) * 0.5
    sine = math.sin(radians)
    cosine = math.cos(radians)
    if channel == "Xrotation":
        return (sine, 0.0, 0.0, cosine)
    if channel == "Yrotation":
        return (0.0, sine, 0.0, cosine)
    if channel == "Zrotation":
        return (0.0, 0.0, sine, cosine)
    raise ValueError(f"unsupported rotation channel '{channel}'")


def rotate_vector(rotation: Quat, vector: Vec3) -> Vec3:
    pure = (vector[0], vector[1], vector[2], 0.0)
    result = quat_multiply(quat_multiply(rotation, pure), quat_inverse(rotation))
    return (result[0], result[1], result[2])


def evaluate_world(motion: BvhMotion, frame: tuple[float, ...]) -> tuple[list[Vec3], list[Quat]]:
    positions: list[Vec3] = []
    rotations: list[Quat] = []
    for index, joint in enumerate(motion.joints):
        translation = list(joint.offset)
        local_rotation: Quat = (0.0, 0.0, 0.0, 1.0)
        for channel_index, channel in enumerate(joint.channels):
            value = frame[joint.channel_offset + channel_index]
            if channel.endswith("position"):
                axis = "XYZ".index(channel[0])
                translation[axis] += value
            else:
                local_rotation = quat_multiply(local_rotation, axis_quaternion(channel, value))
        local_rotation = quat_normalize(local_rotation)
        if joint.parent is None:
            positions.append((translation[0], translation[1], translation[2]))
            rotations.append(local_rotation)
        else:
            parent_rotation = rotations[joint.parent]
            rotated = rotate_vector(parent_rotation,
                                    (translation[0], translation[1], translation[2]))
            parent_position = positions[joint.parent]
            positions.append(tuple(parent_position[axis] + rotated[axis]
                                   for axis in range(3)))  # type: ignore[arg-type]
            rotations.append(quat_normalize(quat_multiply(parent_rotation, local_rotation)))
    return positions, rotations


def local_between(parent_position: Vec3 | None, parent_rotation: Quat | None,
                  position: Vec3, rotation: Quat) -> tuple[Vec3, Quat]:
    if parent_position is None or parent_rotation is None:
        return position, rotation
    inverse = quat_inverse(parent_rotation)
    delta = tuple(position[axis] - parent_position[axis] for axis in range(3))
    return rotate_vector(inverse, delta), quat_normalize(quat_multiply(inverse, rotation))


def continuous_quaternion(previous: Quat | None, value: Quat) -> Quat:
    value = quat_normalize(value)
    if previous is not None and sum(previous[index] * value[index] for index in range(4)) < 0.0:
        return tuple(-component for component in value)  # type: ignore[return-value]
    return value


def clean_values(values: Iterable[float]) -> list[float]:
    return [0.0 if abs(value) < 1.0e-12 else value for value in values]


def build_glb(motion: BvhMotion, *, source_uri: str, source_sha256: str,
              rest_frame: int, start_frame: int, end_frame: int,
              unit_scale: float, clip_name: str) -> bytes:
    if not (0 <= rest_frame < len(motion.frames)):
        raise ValueError("rest frame is outside the BVH motion")
    if not (0 <= start_frame < end_frame <= len(motion.frames)) or end_frame - start_frame < 2:
        raise ValueError("animation frame range must contain at least two frames")
    if not math.isfinite(unit_scale) or unit_scale <= 0.0:
        raise ValueError("BVH unit scale must be positive and finite")

    source_by_name = {joint.name: index for index, joint in enumerate(motion.joints)}
    missing = [source for _, source in HUMANOID_JOINTS if source not in source_by_name]
    if missing:
        raise ValueError(f"BVH is missing humanoid joints: {', '.join(missing)}")
    source_indices = [source_by_name[source] for _, source in HUMANOID_JOINTS]
    output_by_source = {source: output for output, source in enumerate(source_indices)}
    output_parents: list[int | None] = []
    for source in source_indices:
        parent = motion.joints[source].parent
        while parent is not None and parent not in output_by_source:
            parent = motion.joints[parent].parent
        output_parents.append(None if parent is None else output_by_source[parent])

    rest_positions, rest_rotations = evaluate_world(motion, motion.frames[rest_frame])
    nodes: list[dict[str, object]] = []
    for output, (role, _) in enumerate(HUMANOID_JOINTS):
        source = source_indices[output]
        parent = output_parents[output]
        parent_source = None if parent is None else source_indices[parent]
        translation, rotation = local_between(
            None if parent_source is None else rest_positions[parent_source],
            None if parent_source is None else rest_rotations[parent_source],
            rest_positions[source], rest_rotations[source],
        )
        nodes.append({
            "name": role,
            "translation": clean_values(value * unit_scale for value in translation),
            "rotation": clean_values(rotation),
        })
    for child, parent in enumerate(output_parents):
        if parent is not None:
            nodes[parent].setdefault("children", []).append(child)  # type: ignore[union-attr]

    selected = range(start_frame, end_frame)
    times = [(frame - start_frame) * motion.frame_time for frame in selected]
    rotation_tracks: list[list[float]] = [[] for _ in HUMANOID_JOINTS]
    hips_translations: list[float] = []
    previous: list[Quat | None] = [None for _ in HUMANOID_JOINTS]
    for frame_index in selected:
        positions, rotations = evaluate_world(motion, motion.frames[frame_index])
        for output, source in enumerate(source_indices):
            parent = output_parents[output]
            parent_source = None if parent is None else source_indices[parent]
            translation, rotation = local_between(
                None if parent_source is None else positions[parent_source],
                None if parent_source is None else rotations[parent_source],
                positions[source], rotations[source],
            )
            rotation = continuous_quaternion(previous[output], rotation)
            previous[output] = rotation
            rotation_tracks[output].extend(clean_values(rotation))
            if output == 0:
                hips_translations.extend(clean_values(value * unit_scale for value in translation))

    binary = bytearray()
    buffer_views: list[dict[str, int]] = []
    accessors: list[dict[str, object]] = []

    def add_accessor(values: list[float], kind: str, count: int,
                     minimum: list[float] | None = None,
                     maximum: list[float] | None = None) -> int:
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        binary.extend(struct.pack(f"<{len(values)}f", *values))
        view = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": offset,
                             "byteLength": len(binary) - offset})
        accessor: dict[str, object] = {
            "bufferView": view, "componentType": FLOAT_COMPONENT,
            "count": count, "type": kind,
        }
        if minimum is not None:
            accessor["min"] = minimum
        if maximum is not None:
            accessor["max"] = maximum
        accessors.append(accessor)
        return len(accessors) - 1

    time_accessor = add_accessor(times, "SCALAR", len(times), [times[0]], [times[-1]])
    samplers: list[dict[str, object]] = []
    channels: list[dict[str, object]] = []
    for output, track in enumerate(rotation_tracks):
        accessor = add_accessor(track, "VEC4", len(times))
        sampler = len(samplers)
        samplers.append({"input": time_accessor, "output": accessor, "interpolation": "LINEAR"})
        channels.append({"sampler": sampler,
                         "target": {"node": output, "path": "rotation"}})
    hips_accessor = add_accessor(hips_translations, "VEC3", len(times))
    samplers.append({"input": time_accessor, "output": hips_accessor, "interpolation": "LINEAR"})
    channels.append({"sampler": len(samplers) - 1,
                     "target": {"node": 0, "path": "translation"}})

    roots = [index for index, parent in enumerate(output_parents) if parent is None]
    human_bones = {role: {"node": index}
                   for index, (role, _) in enumerate(HUMANOID_JOINTS)}
    document = {
        "accessors": accessors,
        "animations": [{"name": clip_name, "samplers": samplers, "channels": channels}],
        "asset": {"version": "2.0", "generator": "Eidolon deterministic BVH-to-VRMA/1"},
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(binary)}],
        "extensions": {
            "VRMC_vrm_animation": {
                "specVersion": "1.0",
                "humanoid": {"humanBones": human_bones},
            }
        },
        "extensionsRequired": ["VRMC_vrm_animation"],
        "extensionsUsed": ["VRMC_vrm_animation"],
        "extras": {
            "eidolonFixture": {
                "source": source_uri,
                "sourceSha256": source_sha256,
                "restFrame": rest_frame,
                "startFrame": start_frame,
                "endFrameExclusive": end_frame,
                "sourceFrameTime": motion.frame_time,
                "unitScaleMeters": unit_scale,
            }
        },
        "nodes": nodes,
        "scene": 0,
        "scenes": [{"nodes": roots}],
    }
    json_chunk = json.dumps(document, ensure_ascii=False, separators=(",", ":"),
                            sort_keys=True).encode("utf-8")
    json_chunk += b" " * ((-len(json_chunk)) % 4)
    binary.extend(b"\0" * ((-len(binary)) % 4))
    total_length = 12 + 8 + len(json_chunk) + 8 + len(binary)
    return b"".join((
        struct.pack("<III", GLB_MAGIC, GLB_VERSION, total_length),
        struct.pack("<II", len(json_chunk), GLB_JSON_CHUNK), json_chunk,
        struct.pack("<II", len(binary), GLB_BIN_CHUNK), bytes(binary),
    ))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def convert(source: Path, output: Path, *, source_uri: str, source_sha256: str,
            rest_frame: int = 0, start_frame: int = 1, end_frame: int | None = None,
            unit_scale: float = DEFAULT_UNIT_SCALE, clip_name: str = "walk",
            output_sha256: str | None = None) -> None:
    expected = source_sha256.lower()
    if not re.fullmatch(r"[0-9a-f]{64}", expected):
        raise ValueError("source SHA-256 must contain exactly 64 lowercase hexadecimal digits")
    actual = sha256_file(source)
    if actual != expected:
        raise ValueError(f"BVH SHA-256 mismatch: expected {expected}, received {actual}")
    motion = parse_bvh(source)
    end = len(motion.frames) if end_frame is None else end_frame
    payload = build_glb(
        motion, source_uri=source_uri, source_sha256=expected,
        rest_frame=rest_frame, start_frame=start_frame, end_frame=end,
        unit_scale=unit_scale, clip_name=clip_name,
    )
    payload_digest = hashlib.sha256(payload).hexdigest()
    if output_sha256 is not None:
        expected_output = output_sha256.lower()
        if not re.fullmatch(r"[0-9a-f]{64}", expected_output):
            raise ValueError(
                "output SHA-256 must contain exactly 64 lowercase hexadecimal digits"
            )
        if payload_digest != expected_output:
            raise ValueError(
                f"VRMA SHA-256 mismatch: expected {expected_output}, received {payload_digest}"
            )
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=f".{output.name}.", suffix=".partial",
            dir=output.parent, delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(payload)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temporary_path, output)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--source-uri", required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--rest-frame", type=int, default=0)
    parser.add_argument("--start-frame", type=int, default=1)
    parser.add_argument("--end-frame", type=int)
    parser.add_argument("--unit-scale", type=float, default=DEFAULT_UNIT_SCALE)
    parser.add_argument("--clip-name", default="walk")
    parser.add_argument("--output-sha256")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    convert(
        args.source, args.output, source_uri=args.source_uri,
        source_sha256=args.source_sha256.lower(), rest_frame=args.rest_frame,
        start_frame=args.start_frame, end_frame=args.end_frame,
        unit_scale=args.unit_scale, clip_name=args.clip_name,
        output_sha256=args.output_sha256,
    )
    motion = parse_bvh(args.source)
    end = len(motion.frames) if args.end_frame is None else args.end_frame
    print(
        f"built VRMA fixture: {args.output.resolve()} "
        f"frames={end - args.start_frame} duration={(end - args.start_frame - 1) * motion.frame_time:.3f}s "
        f"sha256={sha256_file(args.output)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f"BVH-to-VRMA conversion failed: {error}") from None
