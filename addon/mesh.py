import bpy
import bmesh
from mathutils import Vector, Color
from bpy.props import StringProperty
from bpy_extras.io_utils import ImportHelper

from . import dsts_formats


# ----------------------------------------------------------
# Convert Mesh → Blender Mesh
# ----------------------------------------------------------
def build_blender_mesh(bl_mesh: dsts_formats.Mesh):
    """Convert a C++ Mesh object to a Blender mesh."""

    verts = [x for x in bl_mesh.vertices]
    tris = [x for x in bl_mesh.to_triangle_list()]

    # Create Blender mesh datablock
    mesh = bpy.data.meshes.new(bl_mesh.name)
    bm = bmesh.new()

    # --- Create vertex BMVerts ---
    bm_verts = []
    for v in verts:
        pos = Vector((float(v.position[0]),
                      float(v.position[1]),
                      float(v.position[2])))
        bm_verts.append(bm.verts.new(pos))
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
            v = verts[loop.vert.index]
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
                c = verts[loop.vert.index].color
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
        mesh.normals_split_custom_set_from_vertices(
            [(float(v.normal[0]),
              float(v.normal[1]),
              float(v.normal[2])) for v in verts]
        )

    return mesh