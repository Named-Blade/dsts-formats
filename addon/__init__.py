from . import skeleton
from . import utils
from . import dsts_formats

import bpy

bl_info = {
    "name": "Dsts Formats",
    "author": "Nymic",
    "version" : (0, 0 ,1),
    "blender": (4, 5, 4),
    "category": "Import/Export",
}


class MY_OT_custom_import_operator(bpy.types.Operator):
    bl_idname = "import_scene.my_custom_import"
    bl_label = "My Custom Import"

    def execute(self, context):
        geom = dsts_formats.Geom()
        geom.read_from_file("D:/SteamLibrary/steamapps/common/Digimon Story Time Stranger/gamedata/app_0.dx11/chr748.geom")

        skeleton.import_skeleton(geom.skeleton, utils.unflop)
        self.report({'INFO'}, "Custom import executed!")
        return {'FINISHED'}

def menu_func_import(self, context):
    self.layout.operator(MY_OT_custom_import_operator.bl_idname, text="My Custom Import")

def register():
    bpy.utils.register_class(MY_OT_custom_import_operator)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(MY_OT_custom_import_operator)
