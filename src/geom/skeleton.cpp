#pragma once

#include <cstddef> 
#include <vector>

#include "binary/skeleton.hpp"

namespace dsts::geom
{  
    class Skeleton {
        public:
        binary::SkeletonHeader header;

        std::vector<binary::BoneTransform> bone_transforms;
        std::vector<uint16_t> parent_bones;

        void read(std::istream& f, int skeleton_base, int base = 0){
            f.seekg(base+skeleton_base);
            f.read(reinterpret_cast<char*>(&header), sizeof(binary::SkeletonHeader));

            f.seekg(header.bone_transform_offset + base + skeleton_base + offsetof(binary::SkeletonHeader, bone_transform_offset));
            for (int i = 0; i < header.bone_count; i++) {
                binary::BoneTransform transform;
                f.read(reinterpret_cast<char*>(&transform), sizeof(binary::BoneTransform));
                bone_transforms.push_back(transform);
            }
        }
    };
}
