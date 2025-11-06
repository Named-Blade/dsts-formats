#pragma once

#include <iostream>
#include <cstddef> 
#include <vector>
#include <array>
#include <string>
#include <memory>

#include "../utils/vector.cpp"
#include "../utils/matrix.cpp"
#include "../utils/stream.cpp"
#include "../utils/hash.cpp"
#include "binary/skeleton.hpp"

namespace dsts::geom
{  
    class Bone {
        public:
            uint32_t name_hash;
            std::string name;

            binary::BoneTransform transform;
            
            std::shared_ptr<Bone> parent;
    };

    class FloatChannel {
        public:
            uint32_t name_hash;
            uint32_t array_index;
            uint8_t flags;
    };

    class Skeleton {
        public:
            std::vector<std::shared_ptr<Bone>> bones;
            std::vector<FloatChannel> float_channels;

            uint16_t findBoneIndex();

            void read(std::istream& f, int skeleton_base = 0, int base = 0){
                binary::SkeletonHeader header;

                f.seekg(base+skeleton_base);
                f.read(reinterpret_cast<char*>(&header), sizeof(binary::SkeletonHeader));

                if (header.bone_count > 0) {
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


                    for (int i = 0; i < header.bone_count; i++) {
                        std::shared_ptr<Bone> bone_ptr = std::make_shared<Bone>();

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
                            bones[bone]->parent = bones[parent];
                        }
                    } 
                }

                if (header.float_channel_count > 0) {

                    std::vector<uint32_t> name_hashes(header.float_channel_count);
                    f.seekg(header.float_channel_name_hashes_offset
                        + base 
                        + skeleton_base 
                        + offsetof(binary::SkeletonHeader, float_channel_name_hashes_offset)
                    );
                    f.read(reinterpret_cast<char*>(name_hashes.data()), sizeof(uint32_t) * header.float_channel_count);

                    std::vector<uint32_t> array_indices(header.float_channel_count);
                    f.seekg(header.float_channel_array_indices_offset
                        + base 
                        + skeleton_base 
                        + offsetof(binary::SkeletonHeader, float_channel_array_indices_offset)
                    );
                    f.read(reinterpret_cast<char*>(array_indices.data()), sizeof(uint32_t) * header.float_channel_count);

                    std::vector<uint8_t> flags(header.float_channel_count);
                    f.seekg(header.float_channel_flags_offset
                        + base 
                        + skeleton_base 
                        + offsetof(binary::SkeletonHeader, float_channel_flags_offset)
                    );
                    f.read(reinterpret_cast<char*>(flags.data()), sizeof(uint8_t) * header.float_channel_count);

                    for (int i = 0; i < header.float_channel_count; i++) {
                        FloatChannel channel;

                        channel.name_hash = name_hashes[i];
                        channel.array_index = array_indices[i];
                        channel.flags = flags[i];

                        float_channels.push_back(channel);
                    }

                }
            }

            void write(std::ostream& f, int skeleton_base = 0, int base = 0) {
                binary::SkeletonHeader header;
                f.seekp(skeleton_base + base);
                alignStream(f, 0x10);
                uint64_t skeleton_start = f.tellp();

                header.bone_count = bones.size();
                header.bone_parent_pairs_count = bones.size();
                header.bone_parent_vector_size = 2;

                std::vector<std::array<uint16_t, 2>> bone_parent_pairs(bones.size());
                std::vector<uint16_t> parent_bones(bones.size());
                std::vector<binary::BoneTransform> transforms(bones.size());
                std::vector<uint32_t> bone_name_hashes(bones.size());
                for (int i = 0; i < bones.size() ;i++) {

                    std::array<uint16_t, 2> vector;
                    vector[0] = i;
                    if (bones[i]->parent) {
                        uint16_t parent_index = getIndex(bones, bones[i]->parent);
                        vector[1] = parent_index;
                    } else {
                        constexpr uint16_t NoParent = 0x7FFF;
                        vector[1] = NoParent;
                    }
                    bone_parent_pairs[i] = vector;
                    parent_bones[i] = i;

                    transforms[i] = bones[i]->transform;
                    if (bones[i]->name.length() > 0) {
                        bone_name_hashes[i] = crc32((const uint8_t *)(bones[i]->name.c_str()),bones[i]->name.length());
                    } else {
                        bone_name_hashes[i] = 0;
                    }
                }

                f.seekp(skeleton_start+sizeof(binary::SkeletonHeader));
                f.write(reinterpret_cast<char*>(bone_parent_pairs.data()), sizeof(uint16_t) * 2 * bone_parent_pairs.size());

                alignStream(f, 0x10);
                header.bone_transform_offset = (uint64_t)(f.tellp()) - (skeleton_start + offsetof(binary::SkeletonHeader, bone_transform_offset));
                f.write(reinterpret_cast<char*>(transforms.data()), sizeof(binary::BoneTransform) * transforms.size());

                alignStream(f, 0x10);
                header.parent_bones_offset = (uint64_t)(f.tellp()) - (skeleton_start + offsetof(binary::SkeletonHeader, parent_bones_offset));
                f.write(reinterpret_cast<char*>(parent_bones.data()), sizeof(uint16_t) * parent_bones.size());

                alignStream(f, 0x10);
                uint64_t hashes_section_start = f.tellp();
                header.bone_name_hashes_offset = (uint64_t)(f.tellp()) - (skeleton_start + offsetof(binary::SkeletonHeader, bone_name_hashes_offset));
                f.write(reinterpret_cast<char*>(bone_name_hashes.data()), sizeof(uint32_t) * bone_name_hashes.size());
                
                uint64_t before_pad = f.tellp();
                alignStream(f, 0x10);
                if (before_pad < f.tellp()) {
                    f.seekp((uint64_t)(f.tellp())-0x1);
                    char zero = '\0';
                    f.write(&zero,0x1);
                }
                uint64_t skeleton_end = f.tellp();

                header.skeleton_file_size = skeleton_end - skeleton_start;
                header.hashes_section_bytecount = skeleton_end - hashes_section_start;

                f.seekp(skeleton_start);
                f.write(reinterpret_cast<char*>(&header), sizeof(binary::SkeletonHeader));
            }
    };

    Matrix computeLocalMatrix(const binary::BoneTransform& t){
        Matrix T = Matrix::translation(t.position[0], t.position[1], t.position[2]);
        Matrix R = Matrix::rotationFromQuaternion(t.quaternion[0], t.quaternion[1], t.quaternion[2], t.quaternion[3]);
        Matrix S = Matrix::scale(t.scale[0], t.scale[1], t.scale[2]);

        // DirectX row-major: scale -> rotate -> translate
        return S.multiply(R).multiply(T);
    }

    Matrix computeWorldMatrix(const Bone* bone){
        if(!bone) return Matrix();
        Matrix local = computeLocalMatrix(bone->transform);
        if(bone->parent)
            return computeWorldMatrix(bone->parent.get()).multiply(local);
        return local;
    }

    Matrix computeInverseBindPose(const Bone* bone){
        Matrix world = computeWorldMatrix(bone);
        return world.inverseAffine();
    }

    bool ibpmEqual(const binary::Ibpm& a, const binary::Ibpm& b, float eps = 1e-4f) {
        for (int i = 0; i < 12; ++i) {
            if (std::fabs(a.matrix[i] - b.matrix[i]) > eps) {
                return false;
            }
        }
        return true;
    }

    binary::Ibpm ibpmFromMatrix(Matrix matrix) {
        binary::Ibpm ibpm;
        ibpm.matrix[0] = matrix(0,0);
        ibpm.matrix[1] = matrix(0,1);
        ibpm.matrix[2] = matrix(0,2);
        ibpm.matrix[3] = matrix(0,3);

        ibpm.matrix[4] = matrix(1,0);
        ibpm.matrix[5] = matrix(1,1);
        ibpm.matrix[6] = matrix(1,2);
        ibpm.matrix[7] = matrix(1,3);

        ibpm.matrix[8] = matrix(2,0);
        ibpm.matrix[9] = matrix(2,1);
        ibpm.matrix[10] = matrix(2,2);
        ibpm.matrix[11] = matrix(2,3);
        return ibpm;
    }

    Matrix MatrixFromIbpm(binary::Ibpm ibpm) {
        Matrix matrix;
        matrix(0,0) = ibpm.matrix[0];
        matrix(0,1) = ibpm.matrix[1];
        matrix(0,2) = ibpm.matrix[2];
        matrix(0,3) = ibpm.matrix[3];

        matrix(1,0) = ibpm.matrix[4];
        matrix(1,1) = ibpm.matrix[5];
        matrix(1,2) = ibpm.matrix[6];
        matrix(1,3) = ibpm.matrix[7];

        matrix(2,0) = ibpm.matrix[8];
        matrix(2,1) = ibpm.matrix[9];
        matrix(2,2) = ibpm.matrix[10];
        matrix(2,3) = ibpm.matrix[11];
        return matrix;
    }
}
