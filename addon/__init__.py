from . import skeleton
from . import mesh
from . import material
from . import utils
from . import dsts_formats

from .data import shaderDataNode

from pathlib import Path
import os

import bpy
from bpy_extras.io_utils import ImportHelper
from bpy.props import StringProperty
from bpy.types import Operator

bl_info = {
    "name": "Dsts Formats",
    "author": "Nymic",
    "version" : (0, 0 ,1),
    "blender": (4, 5, 4),
    "category": "Import/Export",
}


class MY_OT_dsts_geom_import_operator(Operator, ImportHelper):
    bl_idname = "import_scene.dsts_geom_import"
    bl_label = "DSTS .geom import"

    # Filter file extensions (optional)
    filename_ext = ".geom"
    filter_glob: StringProperty(
        default="*.geom",
        options={'HIDDEN'}
    )

    def execute(self, context):
        filepath = self.filepath
        geom = dsts_formats.Geom.from_file(filepath)

        # Create a new collection for the imported objects
        new_collection = bpy.data.collections.new(name="Geom")
        context.scene.collection.children.link(new_collection)

        armature_obj = skeleton.import_skeleton(geom.skeleton, new_collection, utils.unflop)

        blender_materials = {}
        for mat_data in geom.materials:
            mat = bpy.data.materials.new(name=mat_data.name)
            mat_data.name = mat.name # fix possible duplicate names

            material.resolve_material(mat, mat_data, str(Path(filepath).parent) + os.sep + "images")

            blender_materials[mat_data.name] = mat

        for mesh_obj in geom.meshes:
            bl_mesh = mesh.build_blender_mesh(mesh_obj)
            obj = bpy.data.objects.new(mesh_obj.name, bl_mesh)

            if mesh_obj.material:
                obj.data.materials.append(blender_materials[mesh_obj.material.name])

            # Link object to the new collection instead of the active one
            new_collection.objects.link(obj)

            # Create vertex groups
            for bone in armature_obj.data.bones:
                obj.vertex_groups.new(name=bone.name)

            # Map palette to bone names
            palette_to_bone = {}
            for idx, bone_data in enumerate(mesh_obj.matrix_palette):
                for bone in armature_obj.data.bones:
                    if bone_data.name == bone.name:
                        palette_to_bone[idx] = bone.name

            # Assign weights
            for v_idx, vert in enumerate(mesh_obj.vertices):
                if mesh_obj.vertices[v_idx].index is not None and mesh_obj.vertices[v_idx].weight is not None:
                    for mi, weight in zip(mesh_obj.vertices[v_idx].index, mesh_obj.vertices[v_idx].weight):
                        bone_name = palette_to_bone[mi]
                        vg = obj.vertex_groups[bone_name]
                        vg.add([v_idx], weight, 'ADD')

            # Parent to armature
            obj.parent = armature_obj
            mod = obj.modifiers.new(name="Armature", type='ARMATURE')
            mod.object = armature_obj

        self.report({'INFO'}, "Custom import executed!")
        return {'FINISHED'}

def menu_func_import(self, context):
    self.layout.operator(MY_OT_dsts_geom_import_operator.bl_idname, text="DSTS .geom import")

def register():
    bpy.utils.register_class(MY_OT_dsts_geom_import_operator)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    shaderDataNode.register()

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(MY_OT_dsts_geom_import_operator)
    shaderDataNode.unregister()
