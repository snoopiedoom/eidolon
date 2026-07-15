"""Repair the canonical Rio FBX assembly and export a runtime GLB."""

import argparse
import json
import os
import struct
import sys
import traceback
from pathlib import Path

import bpy
from mathutils import Matrix


TEXTURE_BY_MATERIAL = {
    "CH0331_Body": "CH0331_Body.png",
    "CH0331_Eyebrow": "CH0331_Face.png",
    "CH0331_EyeMouth": "CH0331_EyeMouth.png",
    "CH0331_Face": "CH0331_Face.png",
    "CH0331_Hair": "CH0331_Hair.png",
    "CH0331_Halo": "CH0331_Halo.png",
}


def force_alpha_cutout(path):
    """Replace glTF alpha blending with deterministic cutouts for layered anime meshes."""
    data = path.read_bytes()
    magic, version, _ = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2:
        raise RuntimeError(f"not a glTF 2 binary: {path}")

    chunks = []
    offset = 12
    while offset < len(data):
        length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunks.append((chunk_type, data[offset : offset + length]))
        offset += length

    json_type = 0x4E4F534A
    document = json.loads(next(payload for kind, payload in chunks if kind == json_type))
    for material in document.get("materials", []):
        if material.get("alphaMode") == "BLEND":
            material["alphaMode"] = "MASK"
            material["alphaCutoff"] = 0.05

    encoded = json.dumps(document, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    chunks = [(kind, encoded if kind == json_type else payload) for kind, payload in chunks]
    total_length = 12 + sum(8 + len(payload) for _, payload in chunks)
    rebuilt = bytearray(struct.pack("<4sII", b"glTF", 2, total_length))
    for kind, payload in chunks:
        rebuilt.extend(struct.pack("<II", len(payload), kind))
        rebuilt.extend(payload)
    path.write_bytes(rebuilt)


def parse_arguments():
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--master", type=Path, required=True)
    parser.add_argument("--halo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mouth-calibration", type=Path)
    return parser.parse_args(arguments)


def require_object(name):
    obj = bpy.data.objects.get(name)
    if obj is None:
        raise RuntimeError(f"required object is missing: {name}")
    return obj


def rebuild_material(material, texture_path):
    if not texture_path.is_file():
        raise RuntimeError(f"material texture is missing: {texture_path}")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = bpy.data.images.load(str(texture_path), check_existing=True)
    texture.interpolation = "Linear"
    shader.inputs["Metallic"].default_value = 0.0
    shader.inputs["Roughness"].default_value = 0.9
    links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    links.new(texture.outputs["Alpha"], shader.inputs["Alpha"])
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    material.diffuse_color = (1.0, 1.0, 1.0, 1.0)
    if hasattr(material, "surface_render_method"):
        material.surface_render_method = "DITHERED"


def rebuild_materials(texture_directory):
    for material_name, texture_name in TEXTURE_BY_MATERIAL.items():
        material = bpy.data.materials.get(material_name)
        if material is not None:
            rebuild_material(material, texture_directory / texture_name)


def preserve_missing_mouth_material(material):
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    image = bpy.data.images.new("Eidolon_MissingMouth", width=1, height=1, alpha=True)
    image.generated_color = (0.0, 0.0, 0.0, 0.0)
    image.pack()
    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    links.new(texture.outputs["Alpha"], shader.inputs["Alpha"])
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    material["eidolon_missing_texture"] = "Character_Mouth_Black.png"
    if hasattr(material, "surface_render_method"):
        material.surface_render_method = "DITHERED"


def import_halo(path, armature):
    previous = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    imported = [obj for obj in bpy.data.objects if obj not in previous and obj.type == "MESH"]
    if len(imported) != 1:
        raise RuntimeError(f"expected one halo mesh, imported {len(imported)}")

    halo = imported[0]
    halo.name = "CH0331_Halo"
    halo.scale = tuple(component * 100.0 for component in halo.scale)
    attachment_bone = "Bip001 Head"
    if attachment_bone not in armature.data.bones:
        raise RuntimeError(f"canonical armature is missing {attachment_bone}")
    # Preserve the repaired world placement in GLB. The runtime binds this node to HaloRoot using
    # the explicit property; Blender's FBX bone-parent round trip is not reliable for this asset.
    halo["eidolon_attach_bone"] = attachment_bone
    return halo


def main():
    args = parse_arguments()
    master = args.master.resolve()
    halo_path = args.halo.resolve()
    output = args.output.resolve()
    for source in (master, halo_path):
        if not source.is_file():
            raise SystemExit(f"source asset does not exist: {source}")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(master))

    armature = require_object("Armature")
    body = require_object("CH0331_Body")
    face = require_object("CH0331_Body.001")
    mouth = require_object("CH0331_Body.002")
    if args.mouth_calibration and args.mouth_calibration.is_file():
        calibration = json.loads(args.mouth_calibration.read_text(encoding="utf-8"))
        offset = calibration.get("offset")
        if not isinstance(offset, list) or len(offset) != 3:
            raise RuntimeError("mouth calibration must contain a three-component offset")
        mouth.matrix_world = Matrix.Translation(tuple(float(value) for value in offset)) @ mouth.matrix_world
    halo = import_halo(halo_path, armature)
    rebuild_materials(master.parent / "Texture2D")
    mouth_material = bpy.data.materials.get("CH0331_Mouth")
    if mouth_material is None:
        raise RuntimeError("required mouth material is missing: CH0331_Mouth")
    mouth_texture = master.parent / "Texture2D" / "Character_Mouth_Black.png"
    if mouth_texture.is_file():
        rebuild_material(mouth_material, mouth_texture)
        mouth_material["eidolon_generated_texture"] = "Character_Mouth_Black.png"
    else:
        preserve_missing_mouth_material(mouth_material)
    mouth["eidolon_expression_mesh"] = "mouth"

    # These source objects are intentionally preserved in FBX but omitted from the neutral GLB:
    # - weapon/outline: exported loose rather than attached to their intended weapon bone
    excluded = {
        "CH0331_Weapon",
        "CH0331_Prop_Outline",
    }
    for name in excluded:
        obj = bpy.data.objects.get(name)
        if obj is not None:
            obj.hide_render = True
            obj.hide_set(True)

    bpy.ops.object.select_all(action="DESELECT")
    for obj in (armature, body, face, mouth, halo):
        obj.hide_set(False)
        obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    print(
        "export selection: " + ", ".join(sorted(obj.name for obj in bpy.context.selected_objects)),
        flush=True,
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=str(output),
        export_format="GLB",
        use_selection=True,
        export_animations=True,
        export_skins=True,
        export_yup=True,
        export_extras=True,
    )
    force_alpha_cutout(output)
    print(f"wrote {output}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except BaseException:
        traceback.print_exc()
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(1)
