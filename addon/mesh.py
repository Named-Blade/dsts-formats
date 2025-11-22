import bpy
import numpy as np
from mathutils import Matrix, Vector
import math
from . import dsts_formats

# ----------------------------------------------------------
# Optimization Helpers
# ----------------------------------------------------------

def get_np_dtype(dtype_str):
    """Maps the C++ string dtype to numpy dtype."""
    mapping = {
        "uByte": np.uint8,
        "sByte": np.int8,
        "uShort": np.uint16,
        "sShort": np.int16,
        "uInt": np.uint32,
        "sInt": np.int32,
        "float": np.float32,
        "float16": np.float16,
    }
    return mapping.get(dtype_str, np.float32)

def extract_attribute(raw_bytes, stride, num_verts, attr):
    """
    Extracts a specific attribute from the interleaved binary buffer.
    Uses attr.count to determine dimensionality (1, 2, 3, 4).
    """
    dtype = get_np_dtype(attr.dtype)
    
    # Calculate total size of this attribute (e.g., 3 floats * 4 bytes = 12 bytes)
    component_count = attr.count 
    attr_size = np.dtype(dtype).itemsize * component_count
    
    # Create a view of the whole buffer as uint8
    byte_view = np.frombuffer(raw_bytes, dtype=np.uint8)
    
    # Reshape to (num_verts, stride) to isolate vertices
    vertex_data = byte_view.reshape(num_verts, stride)
    
    # Slice the columns we need
    attr_bytes = vertex_data[:, attr.offset : attr.offset + attr_size]
    
    # Convert bytes to actual values
    values = np.frombuffer(attr_bytes.tobytes(), dtype=dtype)
    
    # Reshape to (Vertices, Components) if > 1 component
    if component_count > 1:
        values = values.reshape(num_verts, component_count)
        
    return values

# ----------------------------------------------------------
# Convert Mesh → Blender Mesh (Optimized)
# ----------------------------------------------------------
def import_mesh_object(bl_mesh: dsts_formats.Mesh, armature_obj, materials_dict, collection, coord_transform=Matrix.Rotation(math.radians(90), 4, 'X')):
    
    attributes, stride, packed = bl_mesh.pack_vertices()
    num_verts = len(packed) // stride
    attr_map = {a.atype: a for a in attributes}

    # --- GEOMETRY ---
    # No need to pass '3', the attribute knows it has 3 components
    positions = extract_attribute(packed, stride, num_verts, attr_map["position"])
    
    if coord_transform:
        if isinstance(coord_transform, Matrix):
            mat = np.array(coord_transform)
            positions = positions @ mat[:3, :3].T + mat[:3, 3]
        else:
            vecs = [coord_transform(Vector(p)) for p in positions]
            positions = np.array(vecs, dtype=np.float32)

    flat_indices = bl_mesh.get_indices()
    faces = np.array(flat_indices, dtype=np.int32).reshape(-1, 3)

    mesh_data = bpy.data.meshes.new(bl_mesh.name)
    mesh_data.from_pydata(positions, [], faces)
    
    # --- NORMALS ---
    if "normal" in attr_map:
        normals = extract_attribute(packed, stride, num_verts, attr_map["normal"])
        
        if coord_transform and isinstance(coord_transform, Matrix):
            mat_rot = np.array(coord_transform)[:3, :3]
            normals = normals @ mat_rot.T
            norms = np.linalg.norm(normals, axis=1, keepdims=True)
            norms[norms == 0] = 1 
            normals /= norms
            
        mesh_data.normals_split_custom_set_from_vertices(normals)
    
    mesh_data.update(calc_edges=True)

    # --- UVs ---
    if faces.size > 0:
        loop_v_idxs = np.zeros(len(mesh_data.loops), dtype=np.int32)
        mesh_data.loops.foreach_get("vertex_index", loop_v_idxs)
        
        for uv_name in ["uv1", "uv2", "uv3"]:
            if uv_name in attr_map:
                uv_data = extract_attribute(packed, stride, num_verts, attr_map[uv_name])
                uv_layer = mesh_data.uv_layers.new(name=uv_name)
                uv_layer.data.foreach_set("uv", uv_data[loop_v_idxs].reshape(-1))

    # --- OBJECT ---
    obj = bpy.data.objects.new(bl_mesh.name, mesh_data)
    collection.objects.link(obj)

    if bl_mesh.material and bl_mesh.material.name in materials_dict:
        obj.data.materials.append(materials_dict[bl_mesh.material.name])

    # --- WEIGHTS (Fixed for single-bone cases) ---
    if "index" in attr_map and "weight" in attr_map and armature_obj:
        # 1. Map Bones
        palette_map = {}
        if bl_mesh.matrix_palette:
            for idx, bone_data in enumerate(bl_mesh.matrix_palette):
                if bone_data.name in armature_obj.data.bones:
                    palette_map[idx] = bone_data.name
                    if bone_data.name not in obj.vertex_groups:
                        obj.vertex_groups.new(name=bone_data.name)

        # 2. Extract Data
        idx_data = extract_attribute(packed, stride, num_verts, attr_map["index"])
        wgt_data = extract_attribute(packed, stride, num_verts, attr_map["weight"])
        
        # Normalize if uByte
        if wgt_data.dtype == np.uint8:
            wgt_data = wgt_data.astype(np.float32) / 255.0

        # --- FIX: Ensure data is always 2D (N, Comp) ---
        # If count was 1, extract_attribute returned (N,), which causes the error
        if idx_data.ndim == 1:
            idx_data = idx_data.reshape(-1, 1)
        if wgt_data.ndim == 1:
            wgt_data = wgt_data.reshape(-1, 1)

        # 3. Flatten
        flat_weights = wgt_data.flatten()
        flat_indices = idx_data.flatten()
        
        # Now shape[1] is guaranteed to exist
        comp_count = idx_data.shape[1] 
        flat_v_ids = np.repeat(np.arange(num_verts), comp_count)

        # 4. Filter & Assign
        mask = flat_weights > 0.001
        
        valid_weights = flat_weights[mask]
        valid_bone_idxs = flat_indices[mask]
        valid_v_ids = flat_v_ids[mask]

        for i in range(len(valid_weights)):
            palette_idx = valid_bone_idxs[i]
            if palette_idx in palette_map:
                bone_name = palette_map[palette_idx]
                obj.vertex_groups[bone_name].add([int(valid_v_ids[i])], float(valid_weights[i]), 'ADD')

    # --- PARENTING ---
    if armature_obj:
        obj.parent = armature_obj
        mod = obj.modifiers.new(name="Armature", type='ARMATURE')
        mod.object = armature_obj

    return obj