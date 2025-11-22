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

def extract_attribute(raw_bytes, stride, count, attr_meta, component_count=1):
    """
    Extracts a specific attribute from the interleaved binary buffer 
    and returns a numpy array of the correct shape and type.
    """
    dtype = get_np_dtype(attr_meta.dtype)
    
    # Create a view of the whole buffer as uint8
    byte_view = np.frombuffer(raw_bytes, dtype=np.uint8)
    
    # Slice the buffer: start at offset, jump by stride
    # We only take the bytes relevant to this specific attribute type size
    attr_size = np.dtype(dtype).itemsize * component_count
    
    # We need to perform a strided copy because np.frombuffer doesn't support 
    # arbitrary strides on the buffer source directly for different types.
    # 1. Reshape to (count, stride) to isolate vertices
    # 2. Slice the columns we need
    # 3. View as the target type
    
    # Note: This assumes the buffer is perfectly packed/padded to 'stride'
    vertex_data = byte_view.reshape(count, stride)
    attr_bytes = vertex_data[:, attr_meta.offset : attr_meta.offset + attr_size]
    
    # Convert bytes to actual values
    # We must copy here because 'frombuffer' requires contiguous memory for the new dtype
    values = np.frombuffer(attr_bytes.tobytes(), dtype=dtype)
    
    if component_count > 1:
        values = values.reshape(count, component_count)
        
    return values

# ----------------------------------------------------------
# Convert Mesh → Blender Mesh (Optimized)
# ----------------------------------------------------------
def build_blender_mesh(bl_mesh: dsts_formats.Mesh, coord_transform=Matrix.Rotation(math.radians(90), 4, 'X')):
    
    # 1. Get packed binary data from C++
    # attributes: list of MeshAttribute objects
    # stride: int (bytes per vertex)
    # packed: bytes object
    attributes, stride, packed = bl_mesh.pack_vertices()
    
    # Calculate vertex count based on buffer size
    num_verts = len(packed) // stride
    
    # Map attributes by name for easy access
    attr_map = {a.atype: a for a in attributes}
    
    # 2. Extract Positions (Essential for creating mesh)
    if "position" in attr_map:
        positions = extract_attribute(packed, stride, num_verts, attr_map["position"], 3)
    else:
        # Fallback if no positions (unlikely)
        positions = np.zeros((num_verts, 3), dtype=np.float32)

    # Apply Coordinate Transform (Optimized)
    if coord_transform:
        if isinstance(coord_transform, Matrix):
            # Fast matrix multiplication using numpy
            # Convert mathutils Matrix to 4x4 numpy array
            mat = np.array(coord_transform) 
            # Apply rotation/scale (3x3 part) + translation
            # P_new = P @ M_3x3 + T
            positions = positions @ mat[:3, :3].T + mat[:3, 3]
        else:
            # Slow fallback for generic function transforms
            # (It is better to ensure coord_transform is a Matrix passed in)
            vecs = [coord_transform(Vector(p)) for p in positions]
            positions = np.array(vecs, dtype=np.float32)

    # 3. Extract Topology (Faces)
    flat_indices = bl_mesh.get_indices()
    faces_np = np.array(flat_indices, dtype=np.int32)
    faces = faces_np.reshape(-1, 3)

    # 4. Create Mesh Data
    mesh = bpy.data.meshes.new(bl_mesh.name)
    
    # This is the fastest way to create geometry in Blender
    mesh.from_pydata(positions, [], faces)
    
    # 5. Handle Normals
    if "normal" in attr_map:
        normals = extract_attribute(packed, stride, num_verts, attr_map["normal"], 3)
        
        if coord_transform and isinstance(coord_transform, Matrix):
            # Apply 3x3 rotation only (no translation for normals)
            mat_rot = np.array(coord_transform)[:3, :3]
            normals = normals @ mat_rot.T
            # Normalize just in case scale skewed them
            norms = np.linalg.norm(normals, axis=1, keepdims=True)
            # Avoid division by zero
            norms[norms == 0] = 1
            normals /= norms
            
        mesh.normals_split_custom_set_from_vertices(normals)
    
    mesh.update(calc_edges=True)

    # --- PREPARE FOR LOOP MAPPING ---
    # Blender stores UVs and Colors on loops (face corners), C++ has them on vertices.
    # We need to expand vertex data to loop data.
    if "uv1" in attr_map or "color" in attr_map:
        # Get vertex indices for all loops
        # fast way to get loop indices:
        loop_v_idxs = np.zeros(len(mesh.loops), dtype=np.int32)
        mesh.loops.foreach_get("vertex_index", loop_v_idxs)

    # 6. Handle UVs
    for uv_name in ["uv1", "uv2", "uv3"]:
        if uv_name in attr_map:
            # Extract UVs (N, 2)
            uv_data = extract_attribute(packed, stride, num_verts, attr_map[uv_name], 2)
            
            # Create Layer
            uv_layer = mesh.uv_layers.new(name=uv_name)
            
            # Expand (N, 2) -> (NumLoops, 2) using numpy fancy indexing
            loop_uvs = uv_data[loop_v_idxs]
            
            # Flatten to 1D array for foreach_set
            uv_layer.data.foreach_set("uv", loop_uvs.reshape(-1))

    # 7. Handle Colors
    if "color" in attr_map:
        c_attr = attr_map["color"]
        # Determine components (usually 3 or 4)
        # Assuming 3 (RGB) based on original code, but could be 4 (RGBA)
        # Let's try to deduce from dtype size or assume 3 if unclear. 
        # Safe bet: Check if offset+size fits. Usually colors are 4 bytes (RGBA) or 3 floats.
        # Based on original code `float(c[0])...float(c[2]), 1.0`, it implies input is likely RGB.
        
        # Let's assume 3 components for now based on your previous loop code
        comp_count = 3 
        col_data = extract_attribute(packed, stride, num_verts, c_attr, comp_count)
        
        # Normalize if uByte (0-255 -> 0.0-1.0)
        if col_data.dtype == np.uint8:
            col_data = col_data.astype(np.float32) / 255.0
            
        # Blender Color Attributes (ByteColor or FloatColor)
        # Modern Blender uses vertex_colors (Byte) or color_attributes (Float)
        # Let's use the standard vertex_colors
        col_layer = mesh.vertex_colors.new(name="Col")
        
        # Expand Vertex Data -> Loop Data
        loop_cols = col_data[loop_v_idxs]
        
        # If input was RGB, append Alpha=1.0
        if loop_cols.shape[1] == 3:
            ones = np.ones((len(loop_cols), 1), dtype=np.float32)
            loop_cols = np.hstack((loop_cols, ones))
            
        col_layer.data.foreach_set("color", loop_cols.reshape(-1))

    # Final validation
    mesh.validate()
    return mesh