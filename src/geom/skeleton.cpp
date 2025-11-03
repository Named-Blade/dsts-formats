#pragma once

#include <cstddef> 
#include <vector>
#include <string>

#include "binary/skeleton.hpp"

namespace dsts::geom
{  
    class Bone {
        public:
        unsigned int name_hash;
        std::string name;

        binary::BoneTransform transform;
        
        Bone* parent;
    };

    class Skeleton {
        public:
        binary::SkeletonHeader header;

        std::vector<Bone> bones;

        void read(std::istream& f, int skeleton_base, int base = 0){
            f.seekg(base+skeleton_base);
            f.read(reinterpret_cast<char*>(&header), sizeof(binary::SkeletonHeader));

            for (int i = 0; i < header.bone_count; i++) {
                Bone bone;

                f.seekg(header.bone_transform_offset
                    + (i * sizeof(binary::BoneTransform))
                    + base 
                    + skeleton_base 
                    + offsetof(binary::SkeletonHeader, bone_transform_offset)
                );
                f.read(reinterpret_cast<char*>(&bone.transform), sizeof(binary::BoneTransform));

                bones.push_back(bone);
            }
        }
    };
}
