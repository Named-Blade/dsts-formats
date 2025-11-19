import bpy
import bmesh
from mathutils import Vector, Color
from bpy.props import StringProperty
from bpy_extras.io_utils import ImportHelper

from . import dsts_formats


# ----------------------------------------------------------
# Convert Mesh → Blender Mesh
# ----------------------------------------------------------
def build_blender_mesh(bl_mesh: dsts_formats.Mesh, coord_transform=None):
    """Convert a C++ Mesh object to a Blender mesh.
    
    coord_transform: optional function that takes a mathutils.Matrix or Vector
                     and returns a transformed version.
    """
    
    verts = [x for x in bl_mesh.vertices]
    tris = [x for x in bl_mesh.to_triangle_list()]

    # Create Blender mesh datablock
    mesh = bpy.data.meshes.new(bl_mesh.name)
    bm = bmesh.new()

    idx_layer = bm.verts.layers.int.new("idx_layer")

    # --- Create vertex BMVerts ---
    bm_verts = []
    for idx,v in enumerate(verts):
        pos = Vector((float(v.position[0]),
                      float(v.position[1]),
                      float(v.position[2])))
        
        # Apply coordinate transform if given
        if coord_transform is not None:
            pos = coord_transform(pos)
        
        vert = bm.verts.new(pos)
        vert[idx_layer] = idx
        bm_verts.append(vert)
    bm.verts.ensure_lookup_table()

    # --- Create triangles ---
    for t in tris:
        try:
            bm.faces.new((bm_verts[t.i0], bm_verts[t.i1], bm_verts[t.i2]))
        except ValueError:
            # face already exists
            pass

    bm.faces.ensure_lookup_table()

    # --- Create UV layers if present ---
    uv_layers = []
    for uv_attr in ["uv1", "uv2", "uv3"]:
        if hasattr(verts[0], uv_attr):
            uv_layer = bm.loops.layers.uv.new(uv_attr)
            uv_layers.append((uv_attr, uv_layer))

    # Assign UVs
    for face in bm.faces:
        for loop in face.loops:
            v = verts[loop.vert[idx_layer]]
            for attr, layer in uv_layers:
                uv = getattr(v, attr)
                if uv is not None:
                    loop[layer].uv = (
                        float(uv[0]),
                        float(uv[1]),
                    )

    # --- Vertex colors ---
    if hasattr(verts[0], "color"):
        color_layer = bm.loops.layers.color.new("Col")
        for face in bm.faces:
            for loop in face.loops:
                c = verts[loop.vert[idx_layer]].color
                if c is not None:
                    loop[color_layer] = (
                        float(c[0]),
                        float(c[1]),
                        float(c[2]),
                        1.0
                    )

    bm.to_mesh(mesh)
    bm.free()
    mesh.update()

    # --- Normals ---
    if hasattr(verts[0], "normal"):
        normals = [(float(v.normal[0]),
                    float(v.normal[1]),
                    float(v.normal[2])) for v in verts]

        # Apply coordinate transform to normals if matrix-like
        if coord_transform is not None:
            normals = [coord_transform(Vector(n)) for n in normals]

        mesh.normals_split_custom_set_from_vertices(normals)

    return mesh