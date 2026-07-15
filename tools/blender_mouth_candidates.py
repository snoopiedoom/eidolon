"""Render every prepared mouth candidate on the actual Rio rig."""

import argparse
import json
import os
import sys
import traceback
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
from blender_export import rebuild_material, rebuild_materials
from blender_preview import configure_scene


def main():
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--master", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(arguments)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(args.master.resolve()))
    for name in ("CH0331_Weapon", "CH0331_Prop_Outline"):
        obj = bpy.data.objects.get(name)
        if obj is not None:
            bpy.data.objects.remove(obj, do_unlink=True)
    rebuild_materials(args.master.resolve().parent / "Texture2D")
    mouth_material = bpy.data.materials.get("CH0331_Mouth")
    if mouth_material is None:
        raise RuntimeError("required mouth material is missing: CH0331_Mouth")

    args.output.mkdir(parents=True, exist_ok=True)
    configure_scene(args.output / "unused.png", -1.0, 0.24, 0.72)
    scene = bpy.context.scene
    scene.render.resolution_x = 256
    scene.render.resolution_y = 256
    scene.eevee.taa_render_samples = 16

    candidates = json.loads(args.manifest.read_text(encoding="utf-8"))
    for candidate in candidates:
        rebuild_material(mouth_material, Path(candidate["texture"]))
        scene.render.filepath = str(args.output / f"{candidate['label']}.png")
        bpy.ops.render.render(write_still=True)
        print(f"rendered {candidate['label']}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except BaseException:
        traceback.print_exc()
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(1)
