import bpy
from bpy_extras.io_utils import ImportHelper
from bpy.props import StringProperty
from bpy.types import Operator

from .geom import import_geom
from .data import material_nodes

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
        imported_collection = import_geom(context, filepath)

        return {'FINISHED'}

def menu_func_import(self, context):
    self.layout.operator(MY_OT_dsts_geom_import_operator.bl_idname, text="DSTS .geom import")

def register():
    bpy.utils.register_class(MY_OT_dsts_geom_import_operator)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    material_nodes.register()

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(MY_OT_dsts_geom_import_operator)
    material_nodes.unregister()
