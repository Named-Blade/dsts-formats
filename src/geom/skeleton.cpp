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
#include "../utils/quaternion.cpp"
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

    // class FloatChannel {
    //     public:
    //         uint32_t name_hash;
    //         uint32_t array_index;
    //         uint8_t flags;
    // };

    class Skeleton {
        public:
            std::vector<std::shared_ptr<Bone>> bones;
            //std::vector<FloatChannel> float_channels;

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
                    assert(header.float_channel_count == 0);

                    // std::vector<uint32_t> name_hashes(header.float_channel_count);
                    // f.seekg(header.float_channel_name_hashes_offset
                    //     + base 
                    //     + skeleton_base 
                    //     + offsetof(binary::SkeletonHeader, float_channel_name_hashes_offset)
                    // );
                    // f.read(reinterpret_cast<char*>(name_hashes.data()), sizeof(uint32_t) * header.float_channel_count);

                    // std::vector<uint32_t> array_indices(header.float_channel_count);
                    // f.seekg(header.float_channel_array_indices_offset
                    //     + base 
                    //     + skeleton_base 
                    //     + offsetof(binary::SkeletonHeader, float_channel_array_indices_offset)
                    // );
                    // f.read(reinterpret_cast<char*>(array_indices.data()), sizeof(uint32_t) * header.float_channel_count);

                    // std::vector<uint8_t> flags(header.float_channel_count);
                    // f.seekg(header.float_channel_flags_offset
                    //     + base 
                    //     + skeleton_base 
                    //     + offsetof(binary::SkeletonHeader, float_channel_flags_offset)
                    // );
                    // f.read(reinterpret_cast<char*>(flags.data()), sizeof(uint8_t) * header.float_channel_count);

                    // for (int i = 0; i < header.float_channel_count; i++) {
                    //     FloatChannel channel;

                    //     channel.name_hash = name_hashes[i];
                    //     channel.array_index = array_indices[i];
                    //     channel.flags = flags[i];

                    //     float_channels.push_back(channel);
                    // }

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

    Matrix TransformToMatrix(const binary::BoneTransform& t){
        float x = t.quaternion[0];
        float y = t.quaternion[1];
        float z = t.quaternion[2];
        float w = t.quaternion[3];

        float xx = x*x, yy = y*y, zz = z*z;
        float xy = x*y, xz = x*z, yz = y*z;
        float wx = w*x, wy = w*y, wz = w*z;

        Matrix m;

        m(0,0) = (1 - 2*(yy + zz)) * t.scale[0];
        m(0,1) = (2*(xy - wz)) * t.scale[0];
        m(0,2) = (2*(xz + wy)) * t.scale[0];
        m(0,3) = t.position[0];

        m(1,0) = (2*(xy + wz)) * t.scale[1];
        m(1,1) = (1 - 2*(xx + zz)) * t.scale[1];
        m(1,2) = (2*(yz - wx)) * t.scale[1];
        m(1,3) = t.position[1];

        m(2,0) = (2*(xz - wy)) * t.scale[2];
        m(2,1) = (2*(yz + wx)) * t.scale[2];
        m(2,2) = (1 - 2*(xx + yy)) * t.scale[2];
        m(2,3) = t.position[2];

        m(3,0) = 0;
        m(3,1) = 0;
        m(3,2) = 0;
        m(3,3) = 1;

        return m;
    }

    Matrix GetWorldMatrix(const std::shared_ptr<Bone>& bone){
        Matrix local = TransformToMatrix(bone->transform);
        if (bone->parent)
            return GetWorldMatrix(bone->parent).multiply(local);
        else
            return local;
    }

    Matrix ComputeInverseBindPose(const std::shared_ptr<Bone>& bone){
        Matrix world = GetWorldMatrix(bone);
        return world.inverse();
    }

    // Absolute transform: parent + relative
    binary::BoneTransform getAbsoluteTransform(const binary::BoneTransform &rel, const binary::BoneTransform *parent) {
        binary::BoneTransform out = rel;
        if (!parent) return out;

        // Quaternion
        quat_mul(parent->quaternion, rel.quaternion, out.quaternion);

        // Scale (component-wise)
        for (int i = 0; i < 3; ++i)
            out.scale[i] = parent->scale[i] * rel.scale[i];
        out.scale[3] = 1.0f;

        // Position: parent_position + rotated & scaled child position
        float scaled[3] = { rel.position[0]*parent->scale[0], rel.position[1]*parent->scale[1], rel.position[2]*parent->scale[2] };
        quat_rotate(parent->quaternion, scaled, out.position);
        out.position[0] += parent->position[0];
        out.position[1] += parent->position[1];
        out.position[2] += parent->position[2];
        out.position[3] = 1.0f;

        return out;
    }

    // Relative transform: absolute - parent
    binary::BoneTransform getRelativeTransform(const binary::BoneTransform &abs, const binary::BoneTransform *parent) {
        binary::BoneTransform out = abs;
        if (!parent) return out;

        // Quaternion
        float inv[4];
        quat_inverse(parent->quaternion, inv);
        quat_mul(inv, abs.quaternion, out.quaternion);

        // Scale (component-wise)
        for (int i = 0; i < 3; ++i)
            out.scale[i] = abs.scale[i] / parent->scale[i];
        out.scale[3] = 1.0f;

        // Position: inverse rotate & scale
        float pos[3] = { abs.position[0] - parent->position[0],
                        abs.position[1] - parent->position[1],
                        abs.position[2] - parent->position[2] };
        quat_rotate(inv, pos, pos);
        for (int i = 0; i < 3; ++i) pos[i] /= parent->scale[i];
        out.position[0] = pos[0];
        out.position[1] = pos[1];
        out.position[2] = pos[2];
        out.position[3] = 1.0f;

        return out;
    }

    bool transformEqual(const binary::BoneTransform& a, const binary::BoneTransform& b, float eps = 1e-4f) {
        for (int i = 0; i < 4; ++i) {
            if (std::fabs(a.quaternion[i] - b.quaternion[i]) > eps) {
                return false;
            }
        }
        for (int i = 0; i < 4; ++i) {
            if (std::fabs(a.position[i] - b.position[i]) > eps) {
                return false;
            }
        }
        for (int i = 0; i < 4; ++i) {
            if (std::fabs(a.scale[i] - b.scale[i]) > eps) {
                return false;
            }
        }
        return true;
    }

    bool ibpmEqual(const binary::Ibpm& a, const binary::Ibpm& b, float eps = 1e-4f) {
        for (int i = 0; i < 12; ++i) {
            if (std::fabs(a.matrix[i] - b.matrix[i]) > eps) {
                return false;
            }
        }
        return true;
    }
}
