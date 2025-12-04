import bpy
import os
import re
from bpy_extras.io_utils import ImportHelper, ExportHelper
from bpy.props import StringProperty, EnumProperty
from bpy.types import Operator

from .geom import import_geom, export_geom
from .nlst import write_nlst
from .data import material_nodes

bl_info = {
    "name": "Dsts Formats",
    "author": "Nymic_Razor",
    "version" : (0, 1 ,0),
    "blender": (4, 5, 4),
    "category": "Import/Export",
}


class MY_OT_dsts_geom_import_operator(Operator, ImportHelper):
    bl_idname = "import_scene.dsts_geom_import"
    bl_label = "DSTS .geom import"

    filename_ext = ".geom"
    filter_glob: StringProperty(
        default="*.geom",
        options={'HIDDEN'}
    )

    def execute(self, context):
        filepath = self.filepath
        imported_collection = import_geom(context, filepath)

        return {'FINISHED'}
    
class MY_OT_dsts_geom_export_operator(Operator, ExportHelper):
    bl_idname = "export_scene.dsts_geom_export"
    bl_label = "DSTS .geom export"

    def collection_items(self, context):
        return [(col.name, col.name, "") for col in bpy.data.collections if "unknown_0x10" in col]
    
    collection_name: EnumProperty(
        name="Collection",
        description="Select collection to export",
        items=collection_items
    )

    filename_ext = ".geom"
    filter_glob: StringProperty(
        default="*.geom",
        options={'HIDDEN'}
    )

    def execute(self, context): 
        collection = bpy.data.collections.get(self.collection_name)
        if not collection:
            self.report({'ERROR'}, f"Collection '{self.collection_name}' not found")
            return {'CANCELLED'}
        
        geom = export_geom(collection)

        geom.to_file(self.filepath)

        with open(os.path.splitext(self.filepath)[0]+".nlst", "wb") as f:
            f.write(write_nlst(geom).encode("utf-8"))

        return {'FINISHED'}
    
    def invoke(self, context, event):

        collection = bpy.data.collections.get(self.collection_name)
        if collection:
            blend_dir = os.path.dirname(bpy.data.filepath)
            default_name = f"{collection.name}.geom"
            self.filepath = os.path.join(blend_dir, default_name)
        else:
            self.filepath = ""

        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}
    
class ERROR_OT_window(bpy.types.Operator):
    bl_idname = "wm.show_errors_window"
    bl_label = "Import Errors"

    errors: bpy.props.StringProperty()
    window_name: bpy.props.StringProperty(default="Import Errors Window")
    text_name: bpy.props.StringProperty(default="Import Errors")

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        # Duplicate the current area as a new window
        bpy.ops.screen.area_dupli('INVOKE_DEFAULT')
        new_window = context.window_manager.windows[-1]

        # Change the new window's first area to a Text Editor
        for area in new_window.screen.areas:
            area.type = 'TEXT_EDITOR'
            for space in area.spaces:
                if space.type == 'TEXT_EDITOR':
                    # Create a new text block with the errors
                    text = bpy.data.texts.new(name=self.text_name)
                    text.clear()
                    text.write("Errors during import:\n\n"+self.errors)
                    space.text = text

                    # Scroll to top: set first visible line
                    # Blender 4.5 uses text.current_line_index
                    text.current_line_index = 0  # scrolls to first line
                    space.top = 0  # ensure top line is visible

        # Rename the workspace of the new window
        new_window.screen.name = self.window_name

        return {'FINISHED'}

def menu_func_import(self, context):
    self.layout.operator(MY_OT_dsts_geom_import_operator.bl_idname, text="DSTS .geom import")

def menu_func_export(self, context):
    self.layout.operator(MY_OT_dsts_geom_export_operator.bl_idname, text="DSTS .geom export")

def register():
    bpy.utils.register_class(MY_OT_dsts_geom_import_operator)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    bpy.utils.register_class(MY_OT_dsts_geom_export_operator)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)
    bpy.utils.register_class(ERROR_OT_window)
    material_nodes.register()

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(MY_OT_dsts_geom_import_operator)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.utils.unregister_class(MY_OT_dsts_geom_export_operator)
    bpy.utils.unregister_class(ERROR_OT_window)
    material_nodes.unregister()
