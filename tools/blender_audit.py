"""Audit FBX assets with Blender and emit a deterministic JSON inventory."""

import argparse
import json
import os
import sys
import traceback
from collections import Counter
from pathlib import Path

import bpy
from mathutils import Vector


def parse_arguments():
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(arguments)


def rounded_vector(value):
    return [round(float(component), 6) for component in value]


def object_bounds(objects):
    minimum = Vector((float("inf"),) * 3)
    maximum = Vector((float("-inf"),) * 3)
    found = False
    for obj in objects:
        if obj.type != "MESH":
            continue
        for corner in obj.bound_box:
            point = obj.matrix_world @ Vector(corner)
            for axis in range(3):
                minimum[axis] = min(minimum[axis], point[axis])
                maximum[axis] = max(maximum[axis], point[axis])
            found = True
    if not found:
        return None
    return {
        "min": rounded_vector(minimum),
        "max": rounded_vector(maximum),
        "size": rounded_vector(maximum - minimum),
    }


def action_record(action):
    frame_range = getattr(action, "frame_range", (0.0, 0.0))
    return {
        "name": action.name,
        "frame_start": round(float(frame_range[0]), 6),
        "frame_end": round(float(frame_range[1]), 6),
        "fcurves": len(getattr(action, "fcurves", ())),
        "groups": [group.name for group in getattr(action, "groups", ())],
    }


def mesh_record(obj):
    mesh = obj.data
    shape_keys = []
    if mesh.shape_keys is not None:
        shape_keys = [block.name for block in mesh.shape_keys.key_blocks]
    armatures = [
        modifier.object.name
        for modifier in obj.modifiers
        if modifier.type == "ARMATURE" and modifier.object is not None
    ]
    return {
        "object": obj.name,
        "mesh": mesh.name,
        "parent": obj.parent.name if obj.parent else None,
        "parent_type": obj.parent_type if obj.parent else None,
        "parent_bone": obj.parent_bone if obj.parent_type == "BONE" else None,
        "location": rounded_vector(obj.location),
        "rotation": rounded_vector(obj.rotation_euler),
        "scale": rounded_vector(obj.scale),
        "matrix_world": [round(float(value), 6) for row in obj.matrix_world for value in row],
        "vertices": len(mesh.vertices),
        "edges": len(mesh.edges),
        "polygons": len(mesh.polygons),
        "uv_layers": [layer.name for layer in mesh.uv_layers],
        "color_attributes": [attribute.name for attribute in mesh.color_attributes],
        "materials": [slot.material.name if slot.material else None for slot in obj.material_slots],
        "shape_keys": shape_keys,
        "vertex_groups": len(obj.vertex_groups),
        "armatures": armatures,
    }


def armature_record(obj):
    animation = obj.animation_data
    return {
        "object": obj.name,
        "armature": obj.data.name,
        "bones": [bone.name for bone in obj.data.bones],
        "nla_tracks": [track.name for track in animation.nla_tracks] if animation else [],
        "active_action": animation.action.name if animation and animation.action else None,
    }


def audit_loaded_scene(path, root):
    objects = list(bpy.context.scene.objects)
    type_counts = Counter(obj.type for obj in objects)
    images = []
    for image in bpy.data.images:
        source = bpy.path.abspath(image.filepath) if image.filepath else ""
        images.append(
            {
                "name": image.name,
                "source": source,
                "exists": bool(source) and Path(source).is_file(),
                "size": list(image.size),
            }
        )
    return {
        "path": path.relative_to(root).as_posix(),
        "bytes": path.stat().st_size,
        "frame_start": bpy.context.scene.frame_start,
        "frame_end": bpy.context.scene.frame_end,
        "fps": bpy.context.scene.render.fps,
        "unit_system": bpy.context.scene.unit_settings.system,
        "unit_scale": bpy.context.scene.unit_settings.scale_length,
        "object_counts": dict(sorted(type_counts.items())),
        "bounds": object_bounds(objects),
        "meshes": [mesh_record(obj) for obj in objects if obj.type == "MESH"],
        "armatures": [armature_record(obj) for obj in objects if obj.type == "ARMATURE"],
        "actions": [action_record(action) for action in bpy.data.actions],
        "materials": sorted(material.name for material in bpy.data.materials),
        "images": images,
        "cameras": sorted(obj.name for obj in objects if obj.type == "CAMERA"),
        "lights": sorted(obj.name for obj in objects if obj.type == "LIGHT"),
    }


def import_and_audit(path, root):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path))
    return audit_loaded_scene(path, root)


def summary(record):
    meshes = record.get("meshes", [])
    armatures = record.get("armatures", [])
    actions = record.get("actions", [])
    vertices = sum(mesh["vertices"] for mesh in meshes)
    shapes = sum(len(mesh["shape_keys"]) for mesh in meshes)
    return (
        f'{record["path"]}: meshes={len(meshes)} vertices={vertices} '
        f'armatures={len(armatures)} actions={len(actions)} shapes={shapes}'
    )


def main():
    args = parse_arguments()
    root = args.input.resolve()
    output = args.output.resolve()
    if not root.is_dir():
        raise SystemExit(f"model source directory does not exist: {root}")
    files = sorted(root.rglob("*.fbx"), key=lambda path: path.as_posix().lower())
    if not files:
        raise SystemExit(f"no FBX files found under: {root}")
    records = []
    for path in files:
        try:
            record = import_and_audit(path, root)
            record["status"] = "ok"
            print(summary(record), flush=True)
        except Exception as error:
            record = {
                "path": path.relative_to(root).as_posix(),
                "bytes": path.stat().st_size,
                "status": "error",
                "error": str(error),
                "traceback": traceback.format_exc(),
            }
            print(f'{record["path"]}: ERROR {error}', flush=True)
        records.append(record)

    result = {
        "schema": 1,
        "blender": bpy.app.version_string,
        "source": str(root),
        "files": records,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    failures = sum(record["status"] != "ok" for record in records)
    print(f"wrote {output} ({len(records)} files, {failures} failures)", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        exit_code = main()
    except BaseException:
        traceback.print_exc()
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(1)
    if exit_code:
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(exit_code)
