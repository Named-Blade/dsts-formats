#pragma once

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
        }
    };
}
