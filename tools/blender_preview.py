"""Render a deterministic transparent preview of an FBX asset."""

import argparse
import os
import sys
import traceback
from pathlib import Path

import bpy
from mathutils import Vector


def parse_arguments():
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ortho-scale", type=float)
    parser.add_argument("--center-z", type=float)
    return parser.parse_args(arguments)


def bounds():
    points = []
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            points.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    if not points:
        raise RuntimeError("scene has no mesh bounds")
    minimum = Vector(tuple(min(point[axis] for point in points) for axis in range(3)))
    maximum = Vector(tuple(max(point[axis] for point in points) for axis in range(3)))
    return minimum, maximum


def point_at(obj, target):
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def add_area_light(name, location, energy, size, target):
    light_data = bpy.data.lights.new(name=name, type="AREA")
    light_data.energy = energy
    light_data.shape = "DISK"
    light_data.size = size
    light = bpy.data.objects.new(name, light_data)
    bpy.context.scene.collection.objects.link(light)
    light.location = location
    point_at(light, target)


def configure_scene(output, view_axis, ortho_scale=None, center_z=None):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 768
    scene.render.resolution_y = 768
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = True
    scene.render.filepath = str(output)
    scene.render.image_settings.color_depth = "8"

    minimum, maximum = bounds()
    center = (minimum + maximum) * 0.5
    if center_z is not None:
        center.z = center_z
    size = maximum - minimum
    print(
        f"preview bounds: min={tuple(round(value, 4) for value in minimum)} "
        f"max={tuple(round(value, 4) for value in maximum)}",
        flush=True,
    )

    camera_data = bpy.data.cameras.new("AuditCamera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = ortho_scale or max(size.x, size.z) * 1.18
    camera = bpy.data.objects.new("AuditCamera", camera_data)
    scene.collection.objects.link(camera)
    camera.location = center + Vector((0.0, view_axis * max(size.length, 1.0) * 2.5, 0.0))
    point_at(camera, center)
    scene.camera = camera

    add_area_light("Key", center + Vector((-1.5, -2.0, 2.2)), 850.0, 2.0, center)
    add_area_light("Fill", center + Vector((1.8, -1.0, 1.0)), 500.0, 2.5, center)
    add_area_light("Rim", center + Vector((0.5, 1.5, 2.0)), 700.0, 1.5, center)

    if scene.world is None:
        scene.world = bpy.data.worlds.new("AuditWorld")
    scene.world.color = (0.035, 0.035, 0.035)
    scene.view_settings.look = "AgX - Medium High Contrast"


def main():
    args = parse_arguments()
    source = args.input.resolve()
    output = args.output.resolve()
    if not source.is_file():
        raise SystemExit(f"FBX does not exist: {source}")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    if source.suffix.lower() == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(source))
        view_axis = -1.0
    elif source.suffix.lower() in {".glb", ".gltf"}:
        bpy.ops.import_scene.gltf(filepath=str(source))
        view_axis = -1.0
    else:
        raise SystemExit(f"unsupported preview asset: {source}")
    output.parent.mkdir(parents=True, exist_ok=True)
    configure_scene(output, view_axis, args.ortho_scale, args.center_z)
    bpy.ops.render.render(write_still=True)
    print(f"wrote {output}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except BaseException:
        traceback.print_exc()
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(1)
