"""Record Blender's imported FBX material graph without modifying it."""

import argparse
import json
import os
import sys
import traceback
from pathlib import Path

import bpy


def parse_arguments():
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(arguments)


def value_record(value):
    if isinstance(value, (bool, int, float, str)):
        return value
    try:
        return [round(float(component), 8) for component in value]
    except (TypeError, ValueError):
        return str(value)


def node_record(node):
    record = {
        "name": node.name,
        "label": node.label,
        "type": node.bl_idname,
        "inputs": {
            socket.name: value_record(socket.default_value)
            for socket in node.inputs
            if hasattr(socket, "default_value")
        },
    }
    if node.bl_idname == "ShaderNodeTexImage":
        record["image"] = node.image.name if node.image else None
        record["source"] = bpy.path.abspath(node.image.filepath) if node.image else None
        record["extension"] = node.extension
        record["interpolation"] = node.interpolation
        if node.image:
            record["image_properties"] = {
                "alpha_mode": node.image.alpha_mode,
                "colorspace": node.image.colorspace_settings.name,
                "channels": node.image.channels,
                "depth": node.image.depth,
                "is_float": node.image.is_float,
                "size": list(node.image.size),
            }
    if node.bl_idname == "ShaderNodeMapping":
        record["vector_type"] = node.vector_type
    if node.bl_idname == "ShaderNodeUVMap":
        record["uv_map"] = node.uv_map
    return record


def material_record(material):
    properties = {
        "diffuse_color": value_record(material.diffuse_color),
        "surface_render_method": getattr(material, "surface_render_method", None),
        "use_transparency_overlap": getattr(material, "use_transparency_overlap", None),
    }
    if not material.use_nodes or material.node_tree is None:
        return {
            "name": material.name,
            "use_nodes": False,
            "properties": properties,
            "nodes": [],
            "links": [],
        }
    return {
        "name": material.name,
        "use_nodes": True,
        "properties": properties,
        "nodes": [node_record(node) for node in material.node_tree.nodes],
        "links": [
            {
                "from_node": link.from_node.name,
                "from_socket": link.from_socket.name,
                "to_node": link.to_node.name,
                "to_socket": link.to_socket.name,
            }
            for link in material.node_tree.links
        ],
    }


def mesh_record(obj):
    mesh = obj.data
    return {
        "name": obj.name,
        "uv_layers": [
            {
                "name": layer.name,
                "active_render": layer.active_render,
                "minimum": [
                    min((loop.uv[axis] for loop in layer.data), default=0.0)
                    for axis in range(2)
                ],
                "maximum": [
                    max((loop.uv[axis] for loop in layer.data), default=0.0)
                    for axis in range(2)
                ],
            }
            for layer in mesh.uv_layers
        ],
        "color_attributes": [
            {
                "name": attribute.name,
                "domain": attribute.domain,
                "data_type": attribute.data_type,
            }
            for attribute in mesh.color_attributes
        ],
        "material_slots": [slot.material.name if slot.material else None for slot in obj.material_slots],
    }


def main():
    args = parse_arguments()
    source = args.input.resolve()
    output = args.output.resolve()
    if not source.is_file():
        raise RuntimeError(f"FBX does not exist: {source}")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    result = {
        "schema": 1,
        "blender": bpy.app.version_string,
        "source": str(source),
        "materials": [material_record(material) for material in bpy.data.materials],
        "meshes": [mesh_record(obj) for obj in bpy.context.scene.objects if obj.type == "MESH"],
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {output}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except BaseException:
        traceback.print_exc()
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(1)
