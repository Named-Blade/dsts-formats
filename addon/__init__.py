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

        # Create collection
        new_collection = bpy.data.collections.new(name="Geom")
        context.scene.collection.children.link(new_collection)

        # Import Skeleton
        armature_obj = skeleton.import_skeleton(geom.skeleton, new_collection, utils.unflop)

        # Import Materials
        blender_materials = {}
        # Pre-calculate image path base
        base_path = str(Path(filepath).parent) + os.sep + "images"
        
        for mat_data in geom.materials:
            mat = bpy.data.materials.new(name=mat_data.name)
            # Fix name collision handled by Blender
            mat_data.name = mat.name 
            material.resolve_material(mat, mat_data, base_path)
            blender_materials[mat_data.name] = mat

        # Import Objects (Optimized)
        for mesh_obj in geom.meshes:
            # The new function handles mesh build, obj creation, linking, materials, and weights
            mesh.import_mesh_object(
                mesh_obj, 
                armature_obj, 
                blender_materials, 
                new_collection
            )

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
