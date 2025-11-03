#pragma once

#include "enums.hpp"

namespace dsts
{   
    namespace binary 
    {
        #pragma pack(push, 1)

        struct BoneTransform {
            float quaternion[4];
            float position[4];
            float scale[4];
        };

        struct SkeletonHeader {
            char magic[4];
            uint64_t skeleton_file_size;
            uint32_t hashes_section_bytecount;
            uint16_t bone_count;
            uint16_t float_channel_count;
            uint32_t bone_parent_vector_size;

            uint32_t bone_transform_offset;
            uint32_t parent_bones_offset;
            uint32_t bone_name_hashes_offset;
            uint32_t float_channel_array_indices_offset;
            uint32_t float_channel_name_hashes_offset;
            uint32_t float_channel_flags_offset;

            char padding[0xc];
            
            uint32_t bone_parent_pairs_count;
        };

        #pragma pack(pop)
    }
}
