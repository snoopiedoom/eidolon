"""Open an unlit Blender scene for live mouth placement calibration."""

import argparse
import json
import sys
from pathlib import Path

import bpy
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
from blender_export import rebuild_material, rebuild_materials


BASE_MATRIX = None
MOUTH = None
CALIBRATION_PATH = None
MOUTH_GUIDE_NODE = None


def update_mouth(scene, _context):
    if MOUTH is None or BASE_MATRIX is None:
        return
    offset = Matrix.Translation(
        (scene.eidolon_mouth_x, scene.eidolon_mouth_depth, scene.eidolon_mouth_z)
    )
    MOUTH.matrix_world = offset @ BASE_MATRIX


def update_guide(scene, _context):
    if MOUTH_GUIDE_NODE is None:
        return
    MOUTH_GUIDE_NODE.outputs[0].default_value = (
        (1.0, 0.0, 0.35, 1.0) if scene.eidolon_mouth_guide else (0.0, 0.0, 0.0, 1.0)
    )


def frame_face(context, area):
    region = next((item for item in area.regions if item.type == "WINDOW"), None)
    if region is None:
        return
    with context.temp_override(area=area, region=region):
        bpy.ops.view3d.view_axis(type="FRONT", align_active=False)
    view = area.spaces.active.region_3d
    view.view_location = (0.0, 0.0, 0.72)
    view.view_distance = 0.24
    view.view_perspective = "ORTHO"


class EIDOLON_OT_reset_mouth(bpy.types.Operator):
    bl_idname = "eidolon.reset_mouth"
    bl_label = "Reset"

    def execute(self, context):
        context.scene.eidolon_mouth_x = 0.0
        context.scene.eidolon_mouth_z = 0.0
        context.scene.eidolon_mouth_depth = 0.0
        return {"FINISHED"}


class EIDOLON_OT_save_mouth(bpy.types.Operator):
    bl_idname = "eidolon.save_mouth"
    bl_label = "Save calibration"

    def execute(self, context):
        scene = context.scene
        delta = MOUTH.matrix_world.translation - BASE_MATRIX.translation
        payload = {
            "schema": 1,
            "offset": [float(delta.x), float(delta.y), float(delta.z)],
        }
        CALIBRATION_PATH.parent.mkdir(parents=True, exist_ok=True)
        CALIBRATION_PATH.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        self.report({"INFO"}, f"Saved {CALIBRATION_PATH}")
        return {"FINISHED"}


class EIDOLON_OT_frame_face(bpy.types.Operator):
    bl_idname = "eidolon.frame_face"
    bl_label = "Front / frame face"

    def execute(self, context):
        frame_face(context, context.area)
        return {"FINISHED"}


class EIDOLON_PT_mouth(bpy.types.Panel):
    bl_label = "Eidolon mouth"
    bl_idname = "EIDOLON_PT_mouth"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Eidolon"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        layout.label(text="Place the mouth on the live rig")
        layout.label(text="Drag gizmo; MMB orbit; wheel zoom")
        layout.operator("eidolon.frame_face")
        layout.prop(scene, "eidolon_mouth_guide", text="High-contrast mouth")
        layout.prop(scene, "eidolon_mouth_x", text="Horizontal")
        layout.prop(scene, "eidolon_mouth_z", text="Vertical")
        layout.prop(scene, "eidolon_mouth_depth", text="Depth")
        row = layout.row(align=True)
        row.operator("eidolon.reset_mouth")
        row.operator("eidolon.save_mouth")


def make_materials_unlit():
    for material in bpy.data.materials:
        if not material.use_nodes or material.node_tree is None:
            continue
        shader = next(
            (node for node in material.node_tree.nodes if node.bl_idname == "ShaderNodeBsdfPrincipled"),
            None,
        )
        if shader is None:
            continue
        base_link = next(
            (link for link in material.node_tree.links if link.to_node == shader and link.to_socket.name == "Base Color"),
            None,
        )
        if base_link is not None:
            material.node_tree.links.new(base_link.from_socket, shader.inputs["Emission Color"])
            shader.inputs["Emission Strength"].default_value = 0.8
        shader.inputs["Metallic"].default_value = 0.0
        shader.inputs["Roughness"].default_value = 1.0


def configure_mouth_guide(material):
    global MOUTH_GUIDE_NODE
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    shader = next(
        (node for node in nodes if node.bl_idname == "ShaderNodeBsdfPrincipled"), None
    )
    texture = next((node for node in nodes if node.bl_idname == "ShaderNodeTexImage"), None)
    if shader is None or texture is None:
        raise RuntimeError("mouth preview material is missing shader nodes")
    for link in list(shader.inputs["Base Color"].links):
        links.remove(link)
    for link in list(shader.inputs["Emission Color"].links):
        links.remove(link)
    guide = nodes.new("ShaderNodeRGB")
    guide.name = "Eidolon Mouth Guide"
    links.new(guide.outputs[0], shader.inputs["Base Color"])
    links.new(guide.outputs[0], shader.inputs["Emission Color"])
    MOUTH_GUIDE_NODE = guide


def register_ui():
    bpy.utils.register_class(EIDOLON_OT_reset_mouth)
    bpy.utils.register_class(EIDOLON_OT_save_mouth)
    bpy.utils.register_class(EIDOLON_OT_frame_face)
    bpy.utils.register_class(EIDOLON_PT_mouth)
    bpy.types.Scene.eidolon_mouth_x = bpy.props.FloatProperty(
        default=0.0, step=1, precision=5, update=update_mouth
    )
    bpy.types.Scene.eidolon_mouth_z = bpy.props.FloatProperty(
        default=0.0, step=1, precision=5, update=update_mouth
    )
    bpy.types.Scene.eidolon_mouth_depth = bpy.props.FloatProperty(
        default=0.0, step=1, precision=5, update=update_mouth
    )
    bpy.types.Scene.eidolon_mouth_guide = bpy.props.BoolProperty(
        default=True, update=update_guide
    )


def main():
    global BASE_MATRIX, MOUTH, CALIBRATION_PATH
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--master", type=Path, required=True)
    parser.add_argument("--texture", type=Path, required=True)
    parser.add_argument("--calibration", type=Path, required=True)
    args = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(args.master.resolve()))
    for name in ("CH0331_Weapon", "CH0331_Prop_Outline"):
        obj = bpy.data.objects.get(name)
        if obj is not None:
            bpy.data.objects.remove(obj, do_unlink=True)
    rebuild_materials(args.master.resolve().parent / "Texture2D")
    mouth_material = bpy.data.materials.get("CH0331_Mouth")
    if mouth_material is None:
        raise RuntimeError("required mouth material is missing: CH0331_Mouth")
    rebuild_material(mouth_material, args.texture.resolve())
    make_materials_unlit()
    configure_mouth_guide(mouth_material)

    MOUTH = bpy.data.objects.get("CH0331_Body.002")
    if MOUTH is None:
        raise RuntimeError("required mouth object is missing: CH0331_Body.002")
    bpy.ops.object.select_all(action="DESELECT")
    MOUTH.select_set(True)
    bpy.context.view_layer.objects.active = MOUTH
    bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
    BASE_MATRIX = MOUTH.matrix_world.copy()
    CALIBRATION_PATH = args.calibration.resolve()
    register_ui()

    scene = bpy.context.scene
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "Medium High Contrast"
    scene.eidolon_mouth_guide = True
    if CALIBRATION_PATH.is_file():
        saved = json.loads(CALIBRATION_PATH.read_text(encoding="utf-8"))
        offset = saved.get("offset", [0.0, 0.0, 0.0])
        if isinstance(offset, list) and len(offset) == 3:
            scene.eidolon_mouth_x = float(offset[0])
            scene.eidolon_mouth_depth = float(offset[1])
            scene.eidolon_mouth_z = float(offset[2])
    if bpy.context.screen is not None:
        for area in bpy.context.screen.areas:
            if area.type == "VIEW_3D":
                space = area.spaces.active
                space.shading.type = "MATERIAL"
                space.shading.use_scene_world = False
                space.shading.use_scene_lights = False
                space.show_region_ui = True
                space.show_gizmo = True
                space.show_gizmo_object_translate = True
                frame_face(bpy.context, area)


if __name__ == "__main__":
    main()
