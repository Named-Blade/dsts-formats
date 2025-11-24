import bpy
import os
import re

def resolve_material(mat, mat_data, tex_folder):

    mat.use_nodes = True
    mat.blend_method = 'BLEND'
    tree = mat.node_tree
    nodes = tree.nodes
    links = tree.links

    nodes.clear()

    # ---------------------------------------------------------------------
    # Create the group structure
    # ---------------------------------------------------------------------
    group = bpy.data.node_groups.new("DSTS_Data-"+mat.name, 'ShaderNodeTree')

    # Access nodes/links inside the group *immediately*
    g_nodes = group.nodes
    g_links = group.links

    # Group input/output
    group_in  = g_nodes.new("NodeGroupInput")
    group_in.location = (-800, 0)

    group_out = g_nodes.new("NodeGroupOutput")
    group_out.location = (600, 0)

    # Create group output socket (correct 4.x API)
    group.interface.new_socket(
        name="Shader",
        in_out='OUTPUT',
        socket_type='NodeSocketShader'
    )

    # ---------------------------------------------------------------------
    # Create Principled BSDF inside group
    # ---------------------------------------------------------------------
    principled = g_nodes.new("ShaderNodeBsdfPrincipled")
    principled.location = (0, 0)
    if any([s in mat_data.name for s in ["eye_","MTR_line"]]):
        #no idea how to render these, just hide for now
        principled.inputs['Alpha'].default_value = 0.0

    g_links.new(principled.outputs["BSDF"], group_out.inputs["Shader"])

    # DSTS shader data node
    shader_data_node = g_nodes.new(type="DSTS_ShaderData")
    shader_data_node.location = (300, 0)

    for i, shader_name in enumerate((m.name for m in mat_data.shaders)):
        shader_data_node.shader_strings[i].value = shader_name

    # ---------------------------------------------------------------------
    # Texture handling inside the group
    # ---------------------------------------------------------------------
    base_x = -400
    base_y = 0
    y_offset = 0
    y_step = -300

    diffuse_texture_found = False

    is_eye = re.match(".*_f[0-9]{2}(\.[0-9]{3})?$", mat_data.name)

    normal_node = g_nodes.new("ShaderNodeNormalMap")
    normal_node.location = (base_x + 200, base_y + y_offset - 50)
    normal_node.space = 'TANGENT'

    g_links.new(normal_node.outputs["Normal"], principled.inputs["Normal"])

    if is_eye:
        eye_uv = g_nodes.new("ShaderNodeUVMap")
        eye_uv.uv_map = "uv3"

        overlay_eye_1 = g_nodes.new("ShaderNodeMixRGB")
        overlay_eye_alpha = g_nodes.new("ShaderNodeMixRGB")
        overlay_eye_2 = g_nodes.new("ShaderNodeMixRGB")
        overlay_eye_normal = g_nodes.new("ShaderNodeMixRGB")

        overlay_eye_alpha.blend_type = "ADD"

        g_links.new(overlay_eye_1.outputs["Color"], overlay_eye_2.inputs["Color2"])
        g_links.new(overlay_eye_alpha.outputs["Color"], overlay_eye_2.inputs["Fac"])
        g_links.new(overlay_eye_2.outputs["Color"], principled.inputs["Base Color"])
        g_links.new(overlay_eye_normal.outputs["Color"], normal_node.inputs["Color"])

    for uniform in mat_data.uniforms:
        if uniform.uniform_type != "texture":
            continue

        tex_path = os.path.join(tex_folder, uniform.value + ".img")
        if not os.path.exists(tex_path):
            print(f"Warning: texture not found: {tex_path}")
            continue

        tex_node = g_nodes.new("ShaderNodeTexImage")
        tex_node.image = bpy.data.images.load(tex_path)
        tex_node.location = (base_x, base_y + y_offset)
        tex_node.label = "DSTS-" + uniform.parameter_name

        if uniform.parameter_name == "DiffuseColor":
            diffuse_texture_found = True
            tex_node.image.colorspace_settings.name = 'sRGB'
            if is_eye:
                g_links.new(tex_node.outputs["Color"], overlay_eye_2.inputs["Color1"])
            else:
                g_links.new(tex_node.outputs["Color"], principled.inputs["Base Color"])

        elif uniform.parameter_name == "OverlayNormalSampler" and is_eye:
            tex_node.image.colorspace_settings.name = 'sRGB'
            g_links.new(eye_uv.outputs["UV"], tex_node.inputs["Vector"])
            g_links.new(tex_node.outputs["Color"], overlay_eye_1.inputs["Color1"])
            g_links.new(tex_node.outputs["Alpha"], overlay_eye_alpha.inputs["Color1"])

        elif uniform.parameter_name == "OverlayColorSampler3" and is_eye:
            tex_node.image.colorspace_settings.name = 'sRGB'
            g_links.new(eye_uv.outputs["UV"], tex_node.inputs["Vector"])
            g_links.new(tex_node.outputs["Color"], overlay_eye_1.inputs["Color2"])
            g_links.new(tex_node.outputs["Alpha"], overlay_eye_1.inputs["Fac"])
            g_links.new(tex_node.outputs["Alpha"], overlay_eye_alpha.inputs["Color2"])

        elif uniform.parameter_name == "Bumpiness":
            tex_node.image.colorspace_settings.name = 'Non-Color'
            if is_eye:
                g_links.new(tex_node.outputs["Color"], overlay_eye_normal.inputs["Color1"])
            else:
                g_links.new(tex_node.outputs["Color"], normal_node.inputs["Color"])

        elif uniform.parameter_name == "OverlayNormalSampler3" and is_eye:
            tex_node.image.colorspace_settings.name = 'Non-Color'
            g_links.new(eye_uv.outputs["UV"], tex_node.inputs["Vector"])
            g_links.new(tex_node.outputs["Color"], overlay_eye_normal.inputs["Color2"])
            g_links.new(tex_node.outputs["Alpha"], overlay_eye_normal.inputs["Fac"])

        y_offset += y_step

    # Fallback for base color
    if not diffuse_texture_found:
        attr_node = g_nodes.new("ShaderNodeAttribute")
        attr_node.location = (-600, 0)
        attr_node.attribute_name = "Color"
        g_links.new(attr_node.outputs["Color"], principled.inputs["Base Color"])

    # ---------------------------------------------------------------------
    # Instantiate the group in the material tree
    # ---------------------------------------------------------------------
    group_node = nodes.new("ShaderNodeGroup")
    group_node.node_tree = group
    group_node.location = (0, 0)
    
    name = group_node.node_tree.name
    base_width = 60
    char_width = 8
    group_node.width = base_width + len(name) * char_width

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (400, 0)

    links.new(group_node.outputs["Shader"], output.inputs["Surface"])
