import bpy
import os
import re

from pathlib import Path

from . import dsts_formats
from . import data

def layout_columns(node_groups, column_map, x_step=300, y_step=-220):
    """
    Automatically lays out nodes inside a node tree in vertical columns.
    """
    for col_name, nodes in node_groups.items():
        if not nodes:
            continue

        x = column_map[col_name] * x_step
        y = 0

        for n in nodes:
            n.location = (x, y)
            y += y_step

def get_collection_eye_offset_group(collection):
    """
    Creates or retrieves a node group associated with a specific collection.
    Each collection gets its own unique eye offset node group.
    """
    if not collection.get("eye_offset_group_name"):
        # Create a new node group
        group_name = f"{collection.name}_Eye_Offset"
        group = bpy.data.node_groups.new(group_name, 'ShaderNodeTree')

        nodes = group.nodes
        links = group.links

        # Value node for offset
        input_val = nodes.new("ShaderNodeValue")
        input_val.label = "COLLECTION Y OFFSET"
        input_val.location = (-200, 0)
        input_val.outputs[0].default_value = 0.0

        input_val_2 = nodes.new("ShaderNodeValue")
        input_val_2.label = "COLLECTION X OFFSET"
        input_val_2.location = (-200, 200)
        input_val_2.outputs[0].default_value = 0.0

        # Output node
        group_out = nodes.new("NodeGroupOutput")
        group_out.location = (200, 0)

        # Create output socket
        group.interface.new_socket(
            name="Offset Value",
            in_out='OUTPUT',
            socket_type='NodeSocketFloat'
        )

        group.interface.new_socket(
            name="Offset Value X",
            in_out='OUTPUT',
            socket_type='NodeSocketFloat'
        )

        # Connect Value node to output
        links.new(input_val.outputs[0], group_out.inputs[0])
        links.new(input_val_2.outputs[0], group_out.inputs[1])

        # Save reference in collection custom property
        collection["eye_offset_group_name"] = group.name
    else:
        group_name = collection["eye_offset_group_name"]
        group = bpy.data.node_groups.get(group_name)
        if group is None:
            # If the group was deleted, recreate
            del collection["eye_offset_group_name"]
            return get_collection_eye_offset_group(collection)

    return group

def get_image(tex_path, fallback_size=(1024, 1024)):
    tex_path = bpy.path.abspath(tex_path)

    # Check if any existing image matches this path (even if missing on disk)
    for img in bpy.data.images:
        if bpy.path.abspath(img.filepath) == tex_path:
            return img

    # If path exists -> load the file
    if os.path.exists(tex_path):
        return bpy.data.images.load(tex_path)

    # Otherwise create ONE shared placeholder for this path
    name = os.path.basename(tex_path) or tex_path  # stable identifier
    img = bpy.data.images.new(name=name, width=fallback_size[0], height=fallback_size[1])

    # Store the intended filepath so future requests will match this same image
    img.filepath = tex_path

    return img

def resolve_material(collection, mat, mat_data, tex_folder):

    mat.use_nodes = True
    mat.blend_method = 'BLEND'
    tree = mat.node_tree
    nodes = tree.nodes
    links = tree.links

    nodes.clear()

    # ---------------------------------------------------------------------
    # Create the main material group structure
    # ---------------------------------------------------------------------
    group = bpy.data.node_groups.new("DSTS_Data-"+mat.name, 'ShaderNodeTree')

    # Access nodes/links inside the group *immediately*
    g_nodes = group.nodes
    g_links = group.links

    # Group input/output
    group_in  = g_nodes.new("NodeGroupInput")
    group_out = g_nodes.new("NodeGroupOutput")

    # Create group output socket
    group.interface.new_socket(
        name="Shader",
        in_out='OUTPUT',
        socket_type='NodeSocketShader'
    )

    # ---------------------------------------------------------------------
    # Create Principled BSDF inside group
    # ---------------------------------------------------------------------
    principled = g_nodes.new("ShaderNodeBsdfPrincipled")
    if any([s in mat_data.name for s in ["eye_","MTR_line"]]):
        #no idea how to render these, just hide for now
        principled.inputs['Alpha'].default_value = 0.0

    g_links.new(principled.outputs["BSDF"], group_out.inputs["Shader"])

    # DSTS shader data node
    shader_data_node = g_nodes.new(type="DSTS_ShaderData")

    for i, shader_name in enumerate((m.name for m in mat_data.shaders)):
        shader_data_node.shader_strings[i].value = shader_name

    # ---------------------------------------------------------------------
    # Texture handling inside the group
    # ---------------------------------------------------------------------
    is_eye = re.match(".*_f[0-9]{2}(\.[0-9]{3})?$", mat_data.name)

    normal_node = g_nodes.new("ShaderNodeNormalMap")
    normal_node.space = 'TANGENT'

    g_links.new(normal_node.outputs["Normal"], principled.inputs["Normal"])

    # Variables to hold UV logic
    final_eye_vector = None
    
    # Logic to process the offset
    offset_combiner = None
    offset_math = None
    global_offset_node = None

    if is_eye:
        eye_uv = g_nodes.new("ShaderNodeUVMap")
        eye_uv.uv_map = "uv3"

        # --- GLOBAL OFFSET LOGIC ---
        # 1. Add the Shared Global Group Node
        global_offset_node = g_nodes.new("ShaderNodeGroup")
        global_offset_node.node_tree = get_collection_eye_offset_group(collection)
        global_offset_node.label = "Global Offset Control"

        # 2. Create Combine XYZ (Input Y)
        offset_combiner = g_nodes.new("ShaderNodeCombineXYZ")
        
        # 3. Create Vector Math (Add)
        offset_math = g_nodes.new("ShaderNodeVectorMath")
        offset_math.operation = 'ADD'

        # 4. Link Global Node -> Combine XYZ (Y axis)
        g_links.new(global_offset_node.outputs[1], offset_combiner.inputs["X"])
        g_links.new(global_offset_node.outputs[0], offset_combiner.inputs["Y"])
        
        # 5. Link UV -> Math A
        g_links.new(eye_uv.outputs["UV"], offset_math.inputs[0])
        
        # 6. Link Combine XYZ -> Math B
        g_links.new(offset_combiner.outputs["Vector"], offset_math.inputs[1])

        # Store final vector for textures
        final_eye_vector = offset_math.outputs["Vector"]
        # ---------------------------

        overlay_eye_1 = g_nodes.new("ShaderNodeMixRGB")
        overlay_eye_alpha = g_nodes.new("ShaderNodeMath")
        overlay_eye_2 = g_nodes.new("ShaderNodeMixRGB")
        overlay_eye_normal = g_nodes.new("ShaderNodeMixRGB")

        overlay_eye_alpha.operation = "MAXIMUM"

        g_links.new(overlay_eye_1.outputs["Color"], overlay_eye_2.inputs["Color2"])
        g_links.new(overlay_eye_alpha.outputs["Value"], overlay_eye_2.inputs["Fac"])
        g_links.new(overlay_eye_2.outputs["Color"], principled.inputs["Base Color"])
        g_links.new(overlay_eye_normal.outputs["Color"], normal_node.inputs["Color"])

    diffuse_texture_found = False

    for uniform in mat_data.uniforms:
        if uniform.uniform_type == "texture":

            tex_path = os.path.join(tex_folder, uniform.value + ".img")

            tex_node = g_nodes.new("ShaderNodeTexImage")
            tex_node["unknown_0xC"] = uniform.unknown_0xC
            tex_node.image = get_image(tex_path)
            tex_node.label = "DSTS-" + uniform.parameter_name

            if uniform.parameter_name == "DiffuseColor":
                diffuse_texture_found = True
                tex_node.image.colorspace_settings.name = 'sRGB'
                if is_eye:
                    g_links.new(tex_node.outputs["Color"], overlay_eye_2.inputs["Color1"])
                else:
                    g_links.new(tex_node.outputs["Color"], principled.inputs["Base Color"])

            if uniform.parameter_name == "OverlayMaskSampler":
                tex_node.image.colorspace_settings.name = 'sRGB'

                invert_node = g_nodes.new("ShaderNodeInvert")
                sep_node = g_nodes.new("ShaderNodeSeparateColor")

                g_links.new(tex_node.outputs["Color"], sep_node.inputs["Color"])
                g_links.new(sep_node.outputs["Red"], invert_node.inputs["Color"])

                g_links.new(sep_node.outputs["Green"], principled.inputs["Metallic"])
                g_links.new(invert_node.outputs["Color"], principled.inputs["Roughness"])

            if uniform.parameter_name == "LightPixelProj":
                tex_node.image.colorspace_settings.name = 'sRGB'
                g_links.new(tex_node.outputs["Color"], principled.inputs["Emission Color"])
                principled.inputs["Emission Strength"].default_value = 1.0

            elif uniform.parameter_name == "OverlayNormalSampler" and is_eye:
                tex_node.image.colorspace_settings.name = 'sRGB'
                # Connect Offset Vector
                g_links.new(final_eye_vector, tex_node.inputs["Vector"])
                g_links.new(tex_node.outputs["Color"], overlay_eye_1.inputs["Color1"])
                g_links.new(tex_node.outputs["Alpha"], overlay_eye_alpha.inputs[0])

            elif uniform.parameter_name == "OverlayColorSampler3" and is_eye:
                tex_node.image.colorspace_settings.name = 'sRGB'
                # Connect Offset Vector
                g_links.new(final_eye_vector, tex_node.inputs["Vector"])
                g_links.new(tex_node.outputs["Color"], overlay_eye_1.inputs["Color2"])
                g_links.new(tex_node.outputs["Alpha"], overlay_eye_1.inputs["Fac"])
                g_links.new(tex_node.outputs["Alpha"], overlay_eye_alpha.inputs[1])

            elif uniform.parameter_name == "Bumpiness":
                tex_node.image.colorspace_settings.name = 'Non-Color'
                if is_eye:
                    g_links.new(tex_node.outputs["Color"], overlay_eye_normal.inputs["Color1"])
                else:
                    g_links.new(tex_node.outputs["Color"], normal_node.inputs["Color"])

            elif uniform.parameter_name == "OverlayNormalSampler3" and is_eye:
                tex_node.image.colorspace_settings.name = 'Non-Color'
                # Connect Offset Vector
                g_links.new(final_eye_vector, tex_node.inputs["Vector"])
                g_links.new(tex_node.outputs["Color"], overlay_eye_normal.inputs["Color2"])
                g_links.new(tex_node.outputs["Alpha"], overlay_eye_normal.inputs["Fac"])

        elif uniform.uniform_type == "float":
            float_node = g_nodes.new(type="DSTS_ShaderFloatUniform")

            float_node.label = "DSTS-" + uniform.parameter_name
            base_width = 30
            char_width = 8
            float_node.width = base_width + len(float_node.label) * char_width

            for value in uniform.value:
                val = float_node.values.add()
                val.value = value


    # Fallback for base color
    if not diffuse_texture_found:
        attr_node = g_nodes.new("ShaderNodeAttribute")
        attr_node.attribute_name = "Color"
        g_links.new(attr_node.outputs["Color"], principled.inputs["Base Color"])

    for setting in mat_data.settings:
            setting_node = g_nodes.new(type="DSTS_ShaderSetting")

            setting_node.label = "DSTS-" + setting.parameter_name
            base_width = 30
            char_width = 8
            setting_node.width = base_width + len(setting_node.label) * char_width

            setting_node["unknown_0x12"] = setting.unknown_0x12

            for value in list(bytes(setting.value)):
                val = setting_node.values.add()
                val.value = value

    # ------------------------------------------------------------
    # Organize nodes into columns
    # ------------------------------------------------------------
    columns = {
        "shader_data": [],
        "shader_data_2": [],
        "textures": [],
        "utility": [],
        "shader": [],
        "output": []
    }

    # Categorize nodes
    for n in g_nodes:
        if isinstance(n, (data.material_nodes.ShaderDataNode,
                          data.material_nodes.ShaderFloatUniform)):
            columns["shader_data"].append(n)
        elif isinstance(n, data.material_nodes.ShaderSetting):
            columns["shader_data_2"].append(n)
        elif isinstance(n, bpy.types.NodeGroupOutput):
            columns["output"].append(n)
        elif isinstance(n, bpy.types.ShaderNodeTexImage):
            columns["textures"].append(n)
        elif isinstance(n, bpy.types.ShaderNodeBsdfPrincipled):
            columns["shader"].append(n)
        elif isinstance(n, (bpy.types.ShaderNodeNormalMap,
                            bpy.types.ShaderNodeMixRGB,
                            bpy.types.ShaderNodeMath,
                            bpy.types.ShaderNodeUVMap,
                            bpy.types.ShaderNodeCombineXYZ,
                            bpy.types.ShaderNodeVectorMath,
                            bpy.types.ShaderNodeAttribute,
                            bpy.types.ShaderNodeInvert,
                            bpy.types.ShaderNodeSeparateColor)):
            columns["utility"].append(n)
        elif isinstance(n, bpy.types.ShaderNodeGroup) and n.node_tree.name == f"{collection.name}_Eye_Offset":
            columns["shader_data_2"].append(n)

    # Define X-column indices
    column_map = {
        "shader_data": -1,
        "shader_data_2": 0,
        "textures": 1,
        "utility": 2,
        "shader": 3,
        "output": 4
    }

    # Apply layout
    layout_columns(columns, column_map)

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

    mat['DSTS_unknown_0x314'] = mat_data.unknown_0x314
    mat['DSTS_unknown_0x318'] = mat_data.unknown_0x318
    mat['DSTS_unknown_0x31C'] = mat_data.unknown_0x31C
    mat['DSTS_unknown_0x324'] = mat_data.unknown_0x324
    mat['DSTS_unknown_0x326'] = mat_data.unknown_0x326

    links.new(group_node.outputs["Shader"], output.inputs["Surface"])

def export_material(mat):
    mat_out = dsts_formats.Material()

    g_node = next(n for n in mat.node_tree.nodes if n.type == "GROUP" and n.node_tree.name.startswith(f"DSTS_Data-{mat.name}") )
    g_tree = g_node.node_tree

    for node in g_tree.nodes:
        if type(node) == data.material_nodes.ShaderDataNode:
            for i in range(14):
                mat_out.shaders[i].name = node.shader_strings[i].value
        elif type(node) == bpy.types.ShaderNodeTexImage and node.label.startswith("DSTS-"):
            uniform = dsts_formats.ShaderUniform()
            uniform.parameter_name = node.label[5:]
            uniform.value = Path(node.image.name).stem
            uniform.unknown_0xC = node['unknown_0xC']

            mat_out.uniforms.append(uniform)
        elif type(node) == data.material_nodes.ShaderFloatUniform and node.label.startswith("DSTS-"):
            uniform = dsts_formats.ShaderUniform()
            uniform.parameter_name = node.label[5:]
            uniform.value = [v.value for v in node.values]

            mat_out.uniforms.append(uniform)
        elif type(node) == data.material_nodes.ShaderSetting and node.label.startswith("DSTS-"):
            setting = dsts_formats.ShaderSetting()
            setting.parameter_name = node.label[5:]
            setting.value = bytes([v.value for v in node.values])
            setting.unknown_0x12 = node["unknown_0x12"]

            mat_out.settings.append(setting)

    mat_out.name = mat.name

    mat_out.unknown_0x314 = mat['DSTS_unknown_0x314']
    mat_out.unknown_0x318 = mat['DSTS_unknown_0x318']
    mat_out.unknown_0x31C = mat['DSTS_unknown_0x31C']
    mat_out.unknown_0x324 = mat['DSTS_unknown_0x324']
    mat_out.unknown_0x326 = mat['DSTS_unknown_0x326']

    return mat_out