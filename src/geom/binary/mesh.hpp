#pragma once

#include "enums.hpp"

namespace dsts
{   
    namespace binary 
    {
        #pragma pack(push, 1)

        struct Mesh {
            uint64_t vertices_offset;
            uint64_t indices_offset;
            uint64_t matrix_palette_offset;
            uint64_t unknown_0x18;
            uint64_t attributes_offset;

            uint16_t matrix_palette_count;
            uint16_t attribute_count;
            uint16_t bytes_per_vertex;
            uint16_t index_type;

            uint8_t vertex_groups_per_vertex;
            uint8_t flags;
            PrimitiveType primitive_type;

            uint32_t name_hash;
            uint64_t name_offset;

            uint32_t material_idx;
            uint32_t vertex_count;
            uint32_t index_count;

            uint32_t unknown_0x4C;
            uint32_t unknown_0x50;

            float bounding_sphere_radius;
            float centre[3];
            float bbox[3];
            
            uint64_t controller_offset;
            char padding[0x8];
        };

        #pragma pack(pop)
    }
}
