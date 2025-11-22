import bpy
from bpy.types import Node, PropertyGroup

# --- Custom Property Group for strings ---
class ShaderString(PropertyGroup):
    value: bpy.props.StringProperty(name="DSTS Shader String", default="")

# --- Custom Node ---
class ShaderDataNode(Node):
    bl_idname = "DSTS_ShaderData"
    bl_label = "DSTS Shader Data Node"
    bl_icon = 'NODE'

    # Store a list of strings as a property group
    shader_strings: bpy.props.CollectionProperty(type=ShaderString)
    
    string_count: bpy.props.IntProperty(
        name="Count",
        default=14,
        update=lambda self, context: self.resize_strings()
    )
    
    def resize_strings(self):
        while len(self.shader_strings) < self.string_count:
            self.shader_strings.add()
        while len(self.shader_strings) > self.string_count:
            self.shader_strings.remove(len(self.shader_strings)-1)
    
    def init(self, context):
        self.resize_strings()
        self.width = 500
    
    def draw_buttons(self, context, layout):
        #layout.prop(self, "string_count")
        for i, s in enumerate(self.shader_strings):
            layout.prop(s, "value", text=f"Shader {i+1}")
    
    def get_shader_strings(self):
        return [s.value for s in self.shader_strings]

# --- Add menu entry ---
def shader_data_node_menu(self, context):
    self.layout.operator("node.add_node", 
                         text="DSTS Shader Data Node",
                         icon='NODE').type = ShaderDataNode.bl_idname

# --- Register ---
def register():
    bpy.utils.register_class(ShaderString)
    bpy.utils.register_class(ShaderDataNode)
    bpy.types.NODE_MT_add.append(shader_data_node_menu)

def unregister():
    bpy.types.NODE_MT_add.remove(shader_data_node_menu)
    bpy.utils.unregister_class(ShaderDataNode)
    bpy.utils.unregister_class(ShaderString)

if __name__ == "__main__":
    register()