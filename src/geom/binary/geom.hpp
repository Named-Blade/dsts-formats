#pragma once

#include "enums.hpp"

namespace dsts::geom::binary
{  
    #pragma pack(push, 1)

    struct GeomHeader {
        uint32_t version = 316;
        
        uint16_t mesh_count{};
        uint16_t material_count{};
        uint16_t light_count{};
        uint16_t camera_count{};
        uint16_t ibpm_count{};
        char padding[0x2];
        
        uint32_t unknown_0x10{};
        float centre[3]{};
        float bbox[3]{};
        char padding2[0x4]{};
        uint32_t unknown_0x30{};
        uint32_t unknown_0x34{};
        uint64_t skeleton_file_size{};
        char padding3[0x8]{};
        
        uint64_t mesh_offset{};
        uint64_t material_offset{};
        uint64_t light_offset{};
        uint64_t camera_offset{};
        uint64_t ibpm_offset{};
        char padding4[0x8]{};
        
        uint64_t strings_offset{};
        uint64_t clut_offset{};
        char padding5[0x8]{};
        uint64_t name_table_offset{};
        uint64_t skeleton_offset{};
        char padding6[0x8]{};
    };

    #pragma pack(pop)
}
