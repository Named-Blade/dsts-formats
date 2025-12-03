import bpy
from bpy.types import Node, PropertyGroup, Menu

# --- Custom Property Group for strings ---
class ShaderString(PropertyGroup):
    value: bpy.props.StringProperty(name="DSTS Shader String", default="")

class MeshAttributeProperty(bpy.types.PropertyGroup):
    atype: bpy.props.StringProperty(name="Attribute Type")
    dtype: bpy.props.StringProperty(name="Data Type")
    count: bpy.props.IntProperty(name="Count", default=1)
    offset: bpy.props.IntProperty(name="Offset", default=0)

    def __repr__(self):
        return f"<MeshAttribute: {self.atype}>"

# --- Custom Node ---
class ShaderDataNode(Node):
    bl_idname = "DSTS_ShaderData"
    bl_label = "DSTS Shader Data Node"
    bl_icon = 'NODE'

    shader_strings: bpy.props.CollectionProperty(type=ShaderString)
    
    string_count: bpy.props.IntProperty(
        name="Count",
        default=14,
        update=lambda self, context: self.resize()
    )

    bytes_per_vertex: bpy.props.IntProperty(
        name="Bytes"
    )

    attributes: bpy.props.CollectionProperty(
        type=MeshAttributeProperty,
        name="Mesh Attributes"
    )

    active_attribute: bpy.props.IntProperty(name="Active Attribute", default=0)
    
    def resize_strings(self):
        while len(self.shader_strings) < self.string_count:
            self.shader_strings.add()
        while len(self.shader_strings) > self.string_count:
            self.shader_strings.remove(len(self.shader_strings)-1)
    
    def init(self, context):
        self.resize_strings()
        self.width = 500
    
    def draw_buttons(self, context, layout):
        for i, s in enumerate(self.shader_strings):
            layout.prop(s, "value", text=f"Shader {i+1}")

        layout.label(text="Vertex Buffer Layout")
        layout.prop(self, "bytes_per_vertex")
        
        for i, attr in enumerate(self.attributes):
            box = layout.box()
            box.prop(attr, "atype")
            box.prop(attr, "dtype")
            box.prop(attr, "count")
            box.prop(attr, "offset")
    
    def get_shader_strings(self):
        return [s.value for s in self.shader_strings]

# -------------------------
#   CUSTOM SUBMENU
# -------------------------
class NODE_MT_dsts_menu(Menu):
    bl_idname = "NODE_MT_dsts_menu"
    bl_label = "DSTS Nodes"

    def draw(self, context):
        layout = self.layout
        layout.operator(
            "node.add_node",
            text="Shader Data Node",
            icon='NODE'
        ).type = ShaderDataNode.bl_idname


# -------------------------
#   Add submenu to Add Menu
# -------------------------
def add_dsts_submenu(self, context):
    self.layout.menu(NODE_MT_dsts_menu.bl_idname)

class NODE_PT_custom_image_props(bpy.types.Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = 'DSTS Node Data'
    bl_label = "Custom Node Data"

    @classmethod
    def poll(cls, context):
        node = context.active_node
        return node and "unknown_0xC" in node

    def draw(self, context):
        layout = self.layout
        node = context.active_node
        layout.prop(node, '["unknown_0xC"]', text="unknown_0xC")   # <- shows in UI
        layout.label(text=f"Uniform: {node.label[5:]}")

# -------------------------
#   Register
# -------------------------
classes = (
    ShaderString,
    MeshAttributeProperty,
    ShaderDataNode,
    NODE_MT_dsts_menu,
    NODE_PT_custom_image_props
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.NODE_MT_add.append(add_dsts_submenu)

def unregister():
    bpy.types.NODE_MT_add.remove(add_dsts_submenu)
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

if __name__ == "__main__":
    register()
