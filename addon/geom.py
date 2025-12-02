import bpy
import os
from . import skeleton
from . import mesh
from . import material
from . import utils
from . import dsts_formats
from . import data
from pathlib import Path

def import_geom(context, filepath):
    error_state_old = dsts_formats.get_throw_errors()
    error_list_old = dsts_formats.get_error_list()
    dsts_formats.set_throw_errors(False)
    dsts_formats.set_error_list([])

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
        material.resolve_material(new_collection, mat, mat_data, base_path)
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
        #handle material vertex buffer layout
        mat = blender_materials[mesh_obj.material.name]
        attr_list = [
            {"count": attr.count, "offset": attr.offset, "atype": attr.atype, "dtype": attr.dtype}
            for attr in mesh_obj.mesh_attributes
        ]
        g_node = next(n for n in mat.node_tree.nodes if n.type == "GROUP" and n.node_tree.name == f"DSTS_Data-{mat.name}")
        data_node = next(n for n in g_node.node_tree.nodes if type(n) == data.material_nodes.ShaderDataNode)
        if len(data_node.attributes) == 0:
            data_node.bytes_per_vertex = mesh_obj.bytes_per_vertex
            for attr in attr_list:
                at = data_node.attributes.add()
                at.count, at.offset, at.atype, at.dtype = attr.values()


    error_list = dsts_formats.get_error_list()

    if error_list:
        bpy.ops.wm.show_errors_window('INVOKE_DEFAULT', errors="\n\n".join(error_list))

    dsts_formats.set_throw_errors(error_state_old)
    dsts_formats.set_error_list(error_list_old)
    return new_collection