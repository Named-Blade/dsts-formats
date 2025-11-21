#pragma once

#include "enums.hpp"

namespace dsts::geom::binary
{   
    #pragma pack(push, 1)

    struct NameTableHeader {
        uint32_t bone_name_count{};
        uint32_t material_name_count{};
        uint64_t bone_name_offsets_offset{};
        uint64_t material_name_offsets_offset{};
    };

    #pragma pack(pop)
}
