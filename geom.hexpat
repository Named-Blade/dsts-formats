#pragma endian little

import std.io;
import std.ptr;
import std.core;
import std.math;
import std.mem;
import std.sys;
import type.float16;
import type.magic;
import type.size;
import type.byte;

struct GeomHeader {
    u32 version;
    std::assert(version == 316, "version is not 316");
    
    u16 mesh_count;
    u16 material_count;
    u16 light_count;
    u16 camera_count;
    u16 ibpm_count;//inverse bind pose matrices
    std::mem::AlignTo<0x10>;
    
    u32 unknown_0x10;
    float center_point[3];
    float bbox_diag[3];
    padding[0x4];
    u32 unknown_0x30;
    u32 unknown_0x34;
    type::Size<u64> skeleton_size;
    padding[0x8];
    
    u64 mesh_offset;
    u64 material_offset;
    u64 light_offset;
    u64 camera_offset;
    u64 ibpm_offset;
    padding[0x8];
    
    u64 strings_offset;
    u64 clut_offset;//color lookup table
    padding[0x8];
    u64 name_offsets_offset;
    u64 skeleton_offset;
    padding[0x8];
};

enum PrimitiveType : u16 {
    Points,
    Lines,
    Line_loop,
    Line_strip,
    Triangles,
    Triangle_strip,
    Triangle_fan,
};

struct MeshHeader {
    u64 vertices_offset;
    u64 indices_offset;
    u64 matrix_palette_offset;
    u64 unknown_0x18;
    u64 attributes_offset;
    u16 matrix_palette_count;
    u16 attribute_count;
    type::Size<u16> bytes_per_vertex;
    u16 index_type;
    u8 vertex_groups_per_vertex;
    type::Bits flags;
    PrimitiveType primitive_type;
    u32 name_hash;
    u64 name_offset;
    u32 material_idx;
    u32 vertex_count;
    u32 index_count;
    u32 unknown_0x4C;
    u32 unknown_0x50;
    float bounding_sphere_radius;
    float centre[3];
    float bbox[3];
    u64 unknown_mesh_data_offset;
    padding[0x8];
};

enum Atype : u16 {
    Position = 0x1,
    Normal,
    Tangent,
    Binormal,
    UV1,
    UV2,
    UV3,
    unk_8,
    Color,
    Index,
    Weight
};

enum Dtype : u16 {
    uByte,
    sByte,
    Float16,
    uShort,
    Float,
    sShort,
    uInt,
    sInt,
    Float16,
    Float,
};

struct MeshAttribute {
    Atype atype;
    u16 count;
    Dtype dtype;
    u16 offset;
};

struct VertexAttribute<auto Attributes> {
    u32 Base = std::ptr::relative_to_parent(0);
    u32 index = std::core::array_index();
    u32 pos = Base + Attributes[index].offset;
    u32 count = Attributes[index].count;
    Dtype type = Attributes[index].dtype;
    
    match(type) {
        (Dtype::uByte): u8 values[count] @ pos;
        (Dtype::sByte): s8 values[count] @ pos;
        (Dtype::uShort): u16 values[countt] @ pos;
        (Dtype::sShort): s16 values[count] @ pos;
        (Dtype::uInt): u32 values[count] @ pos;
        (Dtype::sInt): s32 values[count] @ pos;
        (Dtype::Float16): type::float16 values[count] @ pos;
        (Dtype::Float): float values[count] @ pos;
    }
};

struct Vertex<auto Attributes, auto AttNum, auto Size>{
    VertexAttribute<Attributes> vertexAttributes[AttNum];
    padding[Size];//not actually padding, just defining size as @ definitions take zero space
};
struct Mesh<auto header> {
    MeshHeader meshHeader;
    
    if (meshHeader.name_offset > 0) char name[] @ header.strings_offset + meshHeader.name_offset;
    
    MeshAttribute meshAttributes[meshHeader.attribute_count] @ meshHeader.attributes_offset;
    //Vertex<meshAttributes, meshHeader.attribute_count, meshHeader.bytes_per_vertex> vertices[meshHeader.vertex_count] @ meshHeader.vertices_offset;   
    //for testing, as fully reading eats memory 
    u32 max_to_read = 10;
    Vertex<meshAttributes, meshHeader.attribute_count, meshHeader.bytes_per_vertex> vertices[std::math::min(max_to_read, meshHeader.vertex_count)] @ meshHeader.vertices_offset;
    u8 vertices_actual[meshHeader.vertex_count * meshHeader.bytes_per_vertex] @ meshHeader.vertices_offset;
        
    u32 matrix_palletes[meshHeader.matrix_palette_count] @ meshHeader.matrix_palette_offset;
    u16 indices[meshHeader.index_count] @ meshHeader.indices_offset;
};

struct Material<auto header> {
    //placeholder
};

struct Light<auto header> {
    //placeholder
};

struct Camera<auto header> {
    //placeholder
};

struct Ibpm<auto header> {
    float matrix[12];//final row [0,0,0,1] is implicit
};

struct ColorData {
    u8 data[4];
};

struct Clut {
    u8 unk1[0x10];
    ColorData color_data[0x100];
    u8 unk2[0xc];
};

struct Name<auto Base, auto Offsets> {
    u32 index = std::core::array_index();
    u64 offset = Offsets[index];
    char name[] @ Base + offset;
};

struct NameOffsets<auto header> {
    u32 bone_name_count;
    u32 material_name_count;
    u64 bone_name_offsets_offset;
    u64 material_name_offsets_offset;
    
    u64 bone_name_offsets[bone_name_count] @ bone_name_offsets_offset;
    u64 material_name_offsets[material_name_count] @ material_name_offsets_offset;
    
    Name<header.strings_offset, bone_name_offsets> bone_names[bone_name_count];
    Name<header.strings_offset, material_name_offsets> material_names[material_name_count];
};

struct BoneParentVector<auto Size>{
    u16 component[Size];
};

struct Quaternion{
    float x;
    float y;
    float z;
    float w;
};

struct BoneTransform {
    Quaternion quaternion;
    float position[4];
    float scale[4];
};

struct Skeleton {
    type::Magic<"60SE"> magic;
    type::Size<u64> skeleton_file_size;
    u32 hashes_section_bytecount;
    u16 bone_count;
    u16 float_channel_count;
    u32 bone_parent_vector_size;
    
    u32 bone_transform_offset;
    BoneTransform bone_transforms[bone_count] @ bone_transform_offset + std::ptr::relative_to_pointer(0) - sizeof(bone_transform_offset);
    
    u32 parent_bones_offset;
    s16 parent_bones[bone_count] @ parent_bones_offset + std::ptr::relative_to_pointer(0) - sizeof(parent_bones_offset);
    
    u32 bone_name_hashes_offset;
    u32 bone_name_hashes[bone_count] @ bone_name_hashes_offset + std::ptr::relative_to_pointer(0) - sizeof(bone_name_hashes_offset);
    
    u32 float_channel_array_indices_offset;
    u32 float_channel_array_indices[float_channel_count] @ float_channel_array_indices_offset + std::ptr::relative_to_pointer(0) - sizeof(float_channel_array_indices_offset);
    
    u32 float_channel_name_hashes_offset;
    u32 float_channel_name_hashes[float_channel_count] @ float_channel_name_hashes_offset + std::ptr::relative_to_pointer(0) - sizeof(float_channel_name_hashes_offset);
    
    u32 float_channel_flags_offset;
    type::Bits float_channel_flags[float_channel_count] @ float_channel_flags_offset + std::ptr::relative_to_pointer(0) - sizeof(float_channel_flags_offset);
    
    padding[0xc];
    
    u32 bone_parent_pairs_count;
    BoneParentVector<bone_parent_vector_size> bone_parent_vectors[bone_parent_pairs_count];
};

struct Geom {
    GeomHeader header;
    
    if (header.mesh_offset > 0) Mesh<header> meshes[header.mesh_count] @ header.mesh_offset;
    if (header.material_offset > 0) Material<header> materials[header.material_count] @ header.material_offset;
    if (header.light_offset > 0) Light<header> lights[header.light_count] @ header.light_offset;
    if (header.camera_offset > 0) Camera<header> cameras[header.camera_count] @ header.camera_offset;
    if (header.ibpm_offset > 0) Ibpm<header> ibpms[header.ibpm_count] @ header.ibpm_offset;
    
    if (header.clut_offset > 0) Clut clut @ header.clut_offset;
    if (header.name_offsets_offset > 0) NameOffsets<header> nameOffsets @ header.name_offsets_offset;
    if (header.skeleton_offset > 0) Skeleton skeleton @ header.skeleton_offset;
};

Geom geom @ 0x0;