#pragma once

#include "enums.hpp"

namespace dsts::geom::binary
{   
    #pragma pack(push, 1)

    struct BoneTransform {
        float quaternion[4] = {0, 0, 0, 1};
        float position[4]   = {0, 0, 0, 1};
        float scale[4]      = {1, 1, 1, 1};
    };

    struct SkeletonHeader {
        char magic[4] = {'6','0','S','E'};
        uint64_t skeleton_file_size         = 0;
        uint32_t hashes_section_bytecount   = 0;
        uint16_t bone_count                 = 0;
        uint16_t float_channel_count        = 0;
        uint32_t bone_parent_vector_size    = 0;

        uint32_t bone_transform_offset                = 0;
        uint32_t parent_bones_offset                  = 0;
        uint32_t bone_name_hashes_offset              = 0;
        uint32_t float_channel_array_indices_offset   = 0;
        uint32_t float_channel_name_hashes_offset     = 0;
        uint32_t float_channel_flags_offset           = 0;

        char padding[0xc] = {};
        
        uint32_t bone_parent_pairs_count = 0;
    };

    struct Ibpm {
        float matrix[12];
    };

    #pragma pack(pop)
}
