from . import skeleton
from . import mesh
from . import utils
from . import dsts_formats

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
        # self.filepath now contains the user-selected path
        filepath = self.filepath

        geom = dsts_formats.Geom.from_file(filepath)

        skeleton.import_skeleton(geom.skeleton, utils.unflop)
        for mesh_obj in geom.meshes:
            bl_mesh = mesh.build_blender_mesh(mesh_obj, utils.unflop)
            obj = bpy.data.objects.new(mesh_obj.name, bl_mesh)
            context.collection.objects.link(obj)

        self.report({'INFO'}, "Custom import executed!")
        return {'FINISHED'}

def menu_func_import(self, context):
    self.layout.operator(MY_OT_dsts_geom_import_operator.bl_idname, text="DSTS .geom import")

def register():
    bpy.utils.register_class(MY_OT_dsts_geom_import_operator)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(MY_OT_dsts_geom_import_operator)
