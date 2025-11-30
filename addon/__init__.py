import bpy
from bpy_extras.io_utils import ImportHelper, ExportHelper
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
    
class MY_OT_dsts_geom_export_operator(Operator, ExportHelper):
    bl_idname = "export_scene.dsts_geom_export"
    bl_label = "DSTS .geom export"

    # Filter file extensions (optional)
    filename_ext = ".geom"
    filter_glob: StringProperty(
        default="*.geom",
        options={'HIDDEN'}
    )

    def execute(self, context):
        from . import dsts_formats as f
        from .skeleton import export_skeleton
        from .mesh import export_mesh_object

        g = f.Geom()
        g.skeleton = export_skeleton([o for o in bpy.data.objects if o.type == "ARMATURE"][0])
        g.materials.append(f.Material())

        for obj in bpy.data.objects:
            if obj.type == "MESH":
                mesh = export_mesh_object(obj, g.skeleton)
                mesh.matrix_palette.append(g.skeleton.bones[0])
                mesh.material = g.materials[0]
                g.meshes.append(mesh)

        g.to_file(self.filepath)

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
    material_nodes.register()

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(MY_OT_dsts_geom_import_operator)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.utils.unregister_class(MY_OT_dsts_geom_export_operator)
    material_nodes.unregister()
