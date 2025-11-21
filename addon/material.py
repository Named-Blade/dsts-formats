import bpy
import os

def resolve_material(mat, mat_data, tex_folder):
    """Create Blender nodes for a material given mat_data and texture folder.

    Args:
        mat (bpy.types.Material): The Blender material to populate
        mat_data: Your custom material data (has .uniforms)
        tex_folder (str): Folder path containing the textures
    """
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    # Clear existing nodes for safety
    nodes.clear()

    # Add Output and Principled BSDF
    output_node = nodes.new(type="ShaderNodeOutputMaterial")
    output_node.location = (400, 0)
    principled_node = nodes.new(type="ShaderNodeBsdfPrincipled")
    principled_node.location = (0, 0)
    links.new(principled_node.outputs["BSDF"], output_node.inputs["Surface"])

    # Track vertical position for node layout
    base_x = -400
    base_y = 0
    y_step = -300
    y_offset = 0

    for uniform in mat_data.uniforms:
        if uniform.uniform_type != "texture":
            continue

        texture_file = uniform.value + ".img"
        tex_path = os.path.join(tex_folder, texture_file)

        if not os.path.exists(tex_path):
            print(f"Warning: texture not found: {tex_path}")
            continue

        # Diffuse Color
        if uniform.parameter_name == "DiffuseColor":
            tex_node = nodes.new("ShaderNodeTexImage")
            tex_node.image = bpy.data.images.load(tex_path)
            tex_node.location = (base_x, base_y + y_offset)
            tex_node.label = "Diffuse Texture"
            tex_node.image.colorspace_settings.name = 'sRGB'
            links.new(tex_node.outputs["Color"], principled_node.inputs["Base Color"])
            y_offset += y_step

        # Normal / Bumpiness
        elif uniform.parameter_name == "Bumpiness":
            tex_node = nodes.new("ShaderNodeTexImage")
            tex_node.image = bpy.data.images.load(tex_path)
            tex_node.location = (base_x, base_y + y_offset)
            tex_node.label = "Normal Map"
            tex_node.image.colorspace_settings.name = 'Non-Color'

            normal_node = nodes.new("ShaderNodeNormalMap")
            normal_node.location = (base_x + 200, base_y + y_offset - 50)
            normal_node.space = 'TANGENT'

            links.new(tex_node.outputs["Color"], normal_node.inputs["Color"])
            links.new(normal_node.outputs["Normal"], principled_node.inputs["Normal"])
            y_offset += y_step
