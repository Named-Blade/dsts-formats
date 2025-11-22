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

    # --- VERTEX COLORS (Color Attributes) ---
    if faces.size > 0:
        # The loop_v_idxs array is already retrieved above for UVs, 
        # but we check again for robustness in case UVs section is skipped.
        if 'loop_v_idxs' not in locals():
             loop_v_idxs = np.zeros(len(mesh_data.loops), dtype=np.int32)
             mesh_data.loops.foreach_get("vertex_index", loop_v_idxs)

        # Assuming the vertex color attribute is named 'color' in the source data.
        if "color" in attr_map:
            color_attr = attr_map["color"]
            color_data = extract_attribute(packed, stride, num_verts, color_attr)
            
            # Ensure color data has 4 components (RGBA) for Blender, padding with alpha=1.0 if necessary
            # Color attributes in Blender are typically 4-component (RGBA) float or byte.
            if color_attr.count == 3:
                # Pad with 1.0 alpha
                alpha_channel = np.ones((num_verts, 1), dtype=color_data.dtype)
                color_data = np.hstack((color_data, alpha_channel))
            elif color_attr.count < 3:
                print(f"Warning: Vertex color attribute 'color' has less than 3 components ({color_attr.count}), skipping.")
                return obj

            # Normalize uByte/uShort data to float if necessary for Blender's color layer
            if color_data.dtype == np.uint8:
                color_data = color_data.astype(np.float32) / 255.0
            elif color_data.dtype == np.uint16:
                 color_data = color_data.astype(np.float32) / 65535.0
            
            # Create a new color attribute on the mesh (Blender 3.x API)
            # Setting 'color' as the attribute name. 
            # type='FLOAT_COLOR' for float data (which we normalized to)
            color_layer = mesh_data.color_attributes.new(
                name='Color', 
                type='FLOAT_COLOR', 
                domain='CORNER' # Color is typically per-face-corner (loop)
            )
            
            # The data is per vertex, but the color layer data is per loop (face corner).
            # We map the per-vertex data to the per-loop indices.
            # Blender expects a flattened array of RGBA values.
            loop_colors = color_data[loop_v_idxs].reshape(-1)

            # Set the data for the new color attribute
            color_layer.data.foreach_set("color", loop_colors)
    
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

    # --- TANGENTS: Compute & Compare ---
    if "tangent" in attr_map and mesh_data.uv_layers:
        # Blender needs tangents enabled
        mesh_data.calc_tangents()

        # Extract custom packed tangents
        packed_tangents = extract_attribute(packed, stride, num_verts, attr_map["tangent"])
        if coord_transform and isinstance(coord_transform, Matrix):
            mat_rot = np.array(coord_transform)[:3, :3]
            if packed_tangents.shape[1] == 4:
                packed_tan_xyz = packed_tangents[:, :3]
                packed_tan_w   = packed_tangents[:, 3]
            else:
                packed_tan_xyz = packed_tangents
                packed_tan_w   = None

            # Apply coord transform (just like normals)
            if coord_transform and isinstance(coord_transform, Matrix):
                mat_rot = np.array(coord_transform)[:3, :3]
                packed_tan_xyz = packed_tan_xyz @ mat_rot.T

            # Re-normalize (safety)
            lens = np.linalg.norm(packed_tan_xyz, axis=1, keepdims=True)
            lens[lens == 0] = 1
            packed_tan_xyz /= lens

            # Recombine if w exists
            if packed_tan_w is not None:
                packed_tangents = np.column_stack([packed_tan_xyz, packed_tan_w])
            else:
                packed_tangents = packed_tan_xyz

        # In many engines, tangent.w stores the sign (±1) used with bitangent reconstruction
        has_w = packed_tangents.shape[1] == 4
        packed_tan_xyz = packed_tangents[:, :3]
        packed_tan_w = packed_tangents[:, 3] if has_w else None

        # Prepare loop mappings
        loop_vertex_indices = np.zeros(len(mesh_data.loops), dtype=np.int32)
        mesh_data.loops.foreach_get("vertex_index", loop_vertex_indices)

        # Blender loop tangents
        bl_tangents = np.zeros((len(mesh_data.loops), 3), dtype=np.float32)
        mesh_data.loops.foreach_get("tangent", bl_tangents.ravel())

        # Blender bitangent sign
        bl_bitangent_signs = np.zeros(len(mesh_data.loops), dtype=np.float32)
        mesh_data.loops.foreach_get("bitangent_sign", bl_bitangent_signs)

        # Build per-vertex tangent average (Blender's data is loop-based)
        accum = np.zeros((num_verts, 3), dtype=np.float32)
        counts = np.zeros(num_verts, dtype=np.int32)

        for i, v in enumerate(loop_vertex_indices):
            accum[v] += bl_tangents[i]
            counts[v] += 1

        # Avoid division by zero
        counts[counts == 0] = 1
        averaged_bl_tangents = accum / counts[:, None]

        # Normalize
        lens = np.linalg.norm(averaged_bl_tangents, axis=1, keepdims=True)
        lens[lens == 0] = 1
        averaged_bl_tangents /= lens

        # --- Compare Packed vs Blender ---
        diff = averaged_bl_tangents - packed_tan_xyz
        errors = np.linalg.norm(diff, axis=1)

        mean_err = float(np.mean(errors))
        max_err = float(np.max(errors))
        idx_max = int(np.argmax(errors))

        print("\n--- Tangent Sanity Check ---")
        print(f"Mean Error: {mean_err:.6f}")
        print(f"Max Error : {max_err:.6f} (vertex {idx_max})")
        print(f"Example:")
        print(f"  Blender: {averaged_bl_tangents[idx_max]}")
        print(f"  Packed : {packed_tan_xyz[idx_max]}")
        print(f"  Δ      : {diff[idx_max]}")

        if has_w:
            print("\nBitangent sign check:")
            # Compare signs at the loop level
            packed_sign_per_loop = packed_tan_w[loop_vertex_indices]
            sign_diff = np.abs(bl_bitangent_signs - packed_sign_per_loop)
            mismatched_signs = np.sum(sign_diff > 0.1)
            print(f"Mismatched bitangent signs: {mismatched_signs} / {len(mesh_data.loops)}")

    
    # --- PARENTING ---
    if armature_obj:
        obj.parent = armature_obj
        mod = obj.modifiers.new(name="Armature", type='ARMATURE')
        mod.object = armature_obj

    return obj