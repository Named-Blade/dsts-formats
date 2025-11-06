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
            binary::BoneTransform transform_actual;
            
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

    template <binary::BoneTransform Bone::*T>
    Matrix computeWorldMatrix(const Bone* bone){
        if(!bone) return Matrix();
        Matrix local = computeLocalMatrix(bone->*T);
        if(bone->parent)
            return computeWorldMatrix<T>(bone->parent.get()).multiply(local);
        return local;
    }

    template <binary::BoneTransform Bone::*T>
    Matrix computeInverseBindPose(const Bone* bone){
        Matrix world = computeWorldMatrix<T>(bone);
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

    binary::BoneTransform recoverBindPose(const Matrix& inverseBindPose, const Matrix* parentInverseBindPose)
    {
        // Step 1: Get world matrix
        Matrix world = inverseBindPose.inverseAffine();

        // Step 2: Compute parent world (if any)
        Matrix parentWorld;
        if (parentInverseBindPose)
            parentWorld = parentInverseBindPose->inverseAffine();
        else
            parentWorld = Matrix::translation(0, 0, 0); // equivalent to identity

        // Step 3: Compute local matrix
        Matrix parentWorldInv = parentWorld.inverseAffine();
        Matrix local = parentWorldInv.multiply(world);

        // Step 4: Extract translation, scale, rotation manually
        binary::BoneTransform t;

        // Translation is last column
        t.position[0] = local.m[0][3];
        t.position[1] = local.m[1][3];
        t.position[2] = local.m[2][3];

        // Extract scale: length of each basis vector
        float sx = sqrtf(local.m[0][0]*local.m[0][0] + local.m[1][0]*local.m[1][0] + local.m[2][0]*local.m[2][0]);
        float sy = sqrtf(local.m[0][1]*local.m[0][1] + local.m[1][1]*local.m[1][1] + local.m[2][1]*local.m[2][1]);
        float sz = sqrtf(local.m[0][2]*local.m[0][2] + local.m[1][2]*local.m[1][2] + local.m[2][2]*local.m[2][2]);
        t.scale[0] = sx; t.scale[1] = sy; t.scale[2] = sz;

        // Normalize rotation part
        Matrix rot;
        rot.m[0][0] = local.m[0][0] / sx;  rot.m[0][1] = local.m[0][1] / sy;  rot.m[0][2] = local.m[0][2] / sz;
        rot.m[1][0] = local.m[1][0] / sx;  rot.m[1][1] = local.m[1][1] / sy;  rot.m[1][2] = local.m[1][2] / sz;
        rot.m[2][0] = local.m[2][0] / sx;  rot.m[2][1] = local.m[2][1] / sy;  rot.m[2][2] = local.m[2][2] / sz;

        // Convert rotation matrix to quaternion
        float trace = rot.m[0][0] + rot.m[1][1] + rot.m[2][2];
        float qw, qx, qy, qz;
        if (trace > 0.0f) {
            float s = sqrtf(trace + 1.0f) * 2.0f;
            qw = 0.25f * s;
            qx = (rot.m[2][1] - rot.m[1][2]) / s;
            qy = (rot.m[0][2] - rot.m[2][0]) / s;
            qz = (rot.m[1][0] - rot.m[0][1]) / s;
        } else if ((rot.m[0][0] > rot.m[1][1]) && (rot.m[0][0] > rot.m[2][2])) {
            float s = sqrtf(1.0f + rot.m[0][0] - rot.m[1][1] - rot.m[2][2]) * 2.0f;
            qw = (rot.m[2][1] - rot.m[1][2]) / s;
            qx = 0.25f * s;
            qy = (rot.m[0][1] + rot.m[1][0]) / s;
            qz = (rot.m[0][2] + rot.m[2][0]) / s;
        } else if (rot.m[1][1] > rot.m[2][2]) {
            float s = sqrtf(1.0f + rot.m[1][1] - rot.m[0][0] - rot.m[2][2]) * 2.0f;
            qw = (rot.m[0][2] - rot.m[2][0]) / s;
            qx = (rot.m[0][1] + rot.m[1][0]) / s;
            qy = 0.25f * s;
            qz = (rot.m[1][2] + rot.m[2][1]) / s;
        } else {
            float s = sqrtf(1.0f + rot.m[2][2] - rot.m[0][0] - rot.m[1][1]) * 2.0f;
            qw = (rot.m[1][0] - rot.m[0][1]) / s;
            qx = (rot.m[0][2] + rot.m[2][0]) / s;
            qy = (rot.m[1][2] + rot.m[2][1]) / s;
            qz = 0.25f * s;
        }

        t.quaternion[0] = qx;
        t.quaternion[1] = qy;
        t.quaternion[2] = qz;
        t.quaternion[3] = qw;

        return t;
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

    void normalize3(float v[3]) {
        float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if(len > 1e-8f) {
            v[0] /= len;
            v[1] /= len;
            v[2] /= len;
        }
    }

    binary::BoneTransform DecomposeMatrix(const Matrix& mat) {
        binary::BoneTransform bt;

        // Extract translation
        bt.position[0] = mat(0,3);
        bt.position[1] = mat(1,3);
        bt.position[2] = mat(2,3);
        bt.position[3] = 1.0f;

        // Extract scale
        float scaleX = std::sqrt(mat(0,0)*mat(0,0) + mat(1,0)*mat(1,0) + mat(2,0)*mat(2,0));
        float scaleY = std::sqrt(mat(0,1)*mat(0,1) + mat(1,1)*mat(1,1) + mat(2,1)*mat(2,1));
        float scaleZ = std::sqrt(mat(0,2)*mat(0,2) + mat(1,2)*mat(1,2) + mat(2,2)*mat(2,2));

        bt.scale[0] = scaleX;
        bt.scale[1] = scaleY;
        bt.scale[2] = scaleZ;
        bt.scale[3] = 1.0f;

        // Remove scale from rotation matrix
        float rot[3][3];
        rot[0][0] = mat(0,0) / scaleX;
        rot[0][1] = mat(0,1) / scaleY;
        rot[0][2] = mat(0,2) / scaleZ;

        rot[1][0] = mat(1,0) / scaleX;
        rot[1][1] = mat(1,1) / scaleY;
        rot[1][2] = mat(1,2) / scaleZ;

        rot[2][0] = mat(2,0) / scaleX;
        rot[2][1] = mat(2,1) / scaleY;
        rot[2][2] = mat(2,2) / scaleZ;

        // Convert rotation matrix to quaternion
        float trace = rot[0][0] + rot[1][1] + rot[2][2];
        if(trace > 0) {
            float s = 0.5f / std::sqrt(trace + 1.0f);
            bt.quaternion[3] = 0.25f / s;
            bt.quaternion[0] = (rot[2][1] - rot[1][2]) * s;
            bt.quaternion[1] = (rot[0][2] - rot[2][0]) * s;
            bt.quaternion[2] = (rot[1][0] - rot[0][1]) * s;
        } else {
            if(rot[0][0] > rot[1][1] && rot[0][0] > rot[2][2]) {
                float s = 2.0f * std::sqrt(1.0f + rot[0][0] - rot[1][1] - rot[2][2]);
                bt.quaternion[3] = (rot[2][1] - rot[1][2]) / s;
                bt.quaternion[0] = 0.25f * s;
                bt.quaternion[1] = (rot[0][1] + rot[1][0]) / s;
                bt.quaternion[2] = (rot[0][2] + rot[2][0]) / s;
            } else if(rot[1][1] > rot[2][2]) {
                float s = 2.0f * std::sqrt(1.0f + rot[1][1] - rot[0][0] - rot[2][2]);
                bt.quaternion[3] = (rot[0][2] - rot[2][0]) / s;
                bt.quaternion[0] = (rot[0][1] + rot[1][0]) / s;
                bt.quaternion[1] = 0.25f * s;
                bt.quaternion[2] = (rot[1][2] + rot[2][1]) / s;
            } else {
                float s = 2.0f * std::sqrt(1.0f + rot[2][2] - rot[0][0] - rot[1][1]);
                bt.quaternion[3] = (rot[1][0] - rot[0][1]) / s;
                bt.quaternion[0] = (rot[0][2] + rot[2][0]) / s;
                bt.quaternion[1] = (rot[1][2] + rot[2][1]) / s;
                bt.quaternion[2] = 0.25f * s;
            }
        }

        return bt;
    }
}
