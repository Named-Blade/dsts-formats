import bpy
import os
from . import skeleton
from . import mesh
from . import material
from . import utils
from . import dsts_formats
from pathlib import Path

def import_geom(context, filepath):
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

    return new_collection