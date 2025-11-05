#pragma once

#include <iostream>

#include <cstddef> 
#include <vector>
#include <array>
#include <string>
#include <memory>

#include "binary/skeleton.hpp"

namespace dsts::geom
{  
    class Bone {
        public:
            uint32_t name_hash;
            std::string name;

            binary::BoneTransform transform;
            
            Bone* parent = nullptr;
    };

    class Skeleton {
        public:
            std::vector<std::unique_ptr<Bone>> bones;

            void read(std::istream& f, int skeleton_base = 0, int base = 0){
                binary::SkeletonHeader header;

                f.seekg(base+skeleton_base);
                f.read(reinterpret_cast<char*>(&header), sizeof(binary::SkeletonHeader));

                assert(header.bone_parent_vector_size == 2);
                std::vector<std::array<uint16_t, 2>> boneParentPairs;
                for (int i = 0; i < header.bone_parent_pairs_count; i++) {
                    std::array<uint16_t, 2> bone_vector;
                    f.read(reinterpret_cast<char*>(bone_vector.data()), sizeof(uint16_t) * 2);
                    boneParentPairs.push_back(bone_vector);
                }

                bones.reserve(header.bone_count);
                for (int i = 0; i < header.bone_count; i++) {
                    std::unique_ptr<Bone> bone_ptr = std::make_unique<Bone>();

                    f.seekg(header.bone_transform_offset
                        + (i * sizeof(binary::BoneTransform))
                        + base 
                        + skeleton_base 
                        + offsetof(binary::SkeletonHeader, bone_transform_offset)
                    );
                    f.read(reinterpret_cast<char*>(&(*bone_ptr).transform), sizeof(binary::BoneTransform));


                    f.seekg(header.bone_name_hashes_offset
                        + (i * sizeof(uint32_t))
                        + base 
                        + skeleton_base 
                        + offsetof(binary::SkeletonHeader, bone_name_hashes_offset)
                    );
                    f.read(reinterpret_cast<char*>(&(*bone_ptr).name_hash), sizeof(uint32_t));

                    bones.push_back(std::move(bone_ptr));
                }

                for (int i = 0; i < header.bone_count; i++) {
                    uint16_t parent_bone;

                    f.seekg(header.parent_bones_offset
                        + (i * sizeof(uint16_t))
                        + base 
                        + skeleton_base 
                        + offsetof(binary::SkeletonHeader, parent_bones_offset)
                    );
                    f.read(reinterpret_cast<char*>(&parent_bone), sizeof(uint16_t));

                    uint16_t bone = boneParentPairs[parent_bone][0];
                    uint16_t parent = boneParentPairs[parent_bone][1];

                    constexpr uint16_t NoParent = 0x7FFF;
                    if (parent != NoParent) {
                        bones[bone]->parent = bones[parent].get();
                    }
                }
                
            }
    };
}
