import bpy
import os
import re
from bpy_extras.io_utils import ImportHelper, ExportHelper
from bpy.props import StringProperty, EnumProperty
from bpy.types import Operator

from .geom import import_geom
from .nlst import write_nlst
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
        return [(col.name, col.name, "") for col in bpy.data.collections]
    
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
        from . import dsts_formats as f
        from .skeleton import export_skeleton
        from .material import export_material
        from .mesh import export_mesh_object
        

        g = f.Geom()

        collection = bpy.data.collections.get(self.collection_name)
        if not collection:
            self.report({'ERROR'}, f"Collection '{self.collection_name}' not found")
            return {'CANCELLED'}

        armatures = [o for o in collection.objects if o.type == "ARMATURE"]
        if not armatures:
            self.report({'ERROR'}, "No armature found in collection")
            return {'CANCELLED'}
        
        g.skeleton = export_skeleton(armatures[0])

        name_to_mat = {}
        for mat in {o.data.materials[0] for o in collection.objects if o.type == "MESH"}:
            mat_data = export_material(mat)
            g.materials.append(mat_data)
            name_to_mat[mat_data.name] = mat_data

        for obj in [o for o in collection.objects if o.type == "MESH"]:
            mesh = export_mesh_object(obj, g.skeleton)
            mat_name = obj.data.materials[0].name
            mesh.material = name_to_mat[mat_name]
            g.meshes.append(mesh)

        for obj in [*g.skeleton.bones] + [*g.materials] + [*g.meshes]:
            obj.name = re.sub("(\.[0-9]{3})?$", "", obj.name)

        g.to_file(self.filepath)

        with open(os.path.splitext(self.filepath)[0]+".nlst", "wb") as f:
            f.write(write_nlst(g).encode("utf-8"))

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
