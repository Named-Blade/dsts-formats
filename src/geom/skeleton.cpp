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
                std::vector<std::array<uint16_t, 2>> boneParentPairs(header.bone_parent_pairs_count);
                if (!boneParentPairs.empty()) {
                    f.read(reinterpret_cast<char*>(boneParentPairs.data()), sizeof(uint16_t) * 2 * boneParentPairs.size());
                }

                std::vector<binary::BoneTransform> transforms(header.bone_count);
                f.seekg(header.bone_transform_offset
                    + base 
                    + skeleton_base 
                    + offsetof(binary::SkeletonHeader, bone_transform_offset)
                );
                f.read(reinterpret_cast<char*>(transforms.data()), sizeof(binary::BoneTransform) * header.bone_count);

                std::vector<uint32_t> name_hashes(header.bone_count);
                f.seekg(header.bone_name_hashes_offset
                    + base 
                    + skeleton_base 
                    + offsetof(binary::SkeletonHeader, bone_name_hashes_offset)
                );
                f.read(reinterpret_cast<char*>(name_hashes.data()), sizeof(uint32_t) * header.bone_count);


                bones.reserve(header.bone_count);
                for (int i = 0; i < header.bone_count; i++) {
                    std::unique_ptr<Bone> bone_ptr = std::make_unique<Bone>();

                    bone_ptr->transform = transforms[i];
                    bone_ptr->name_hash = name_hashes[i];

                    bones.push_back(std::move(bone_ptr));
                }

                std::vector<uint16_t> parent_bones(header.bone_count);
                f.seekg(header.parent_bones_offset
                    + base 
                    + skeleton_base 
                    + offsetof(binary::SkeletonHeader, parent_bones_offset)
                );
                f.read(reinterpret_cast<char*>(parent_bones.data()), sizeof(uint16_t) * header.bone_count);

                for (int i = 0; i < header.bone_count; i++) {

                    uint16_t bone = boneParentPairs[parent_bones[i]][0];
                    uint16_t parent = boneParentPairs[parent_bones[i]][1];

                    constexpr uint16_t NoParent = 0x7FFF;
                    if (parent != NoParent) {
                        bones[bone]->parent = bones[parent].get();
                    }
                } 
            }
    };
}
