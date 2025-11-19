#pragma once

#include <functional>
#include <variant>

#include "binary/clut.hpp"
#include "binary/enums.hpp"
#include "binary/geom.hpp"
#include "binary/material.hpp"
#include "binary/mesh.hpp"
#include "binary/name_table.hpp"
#include "binary/skeleton.hpp"

#include "skeleton.cpp"
#include "mesh.cpp"
#include "../utils/hash.cpp"

namespace dsts::geom
{  
    class Geom {
        public:
            uint32_t unknown_0x10 = 0;
            uint32_t unknown_0x30 = 0;
            uint32_t unknown_0x34 = 0;

            Skeleton skeleton;
            std::vector<Mesh> meshes;

            void read(std::istream& f, int base = 0){
                binary::GeomHeader header;

                f.seekg(base);
                f.read(reinterpret_cast<char*>(&header), sizeof(binary::GeomHeader));

                unknown_0x10 = header.unknown_0x10;
                unknown_0x30 = header.unknown_0x30;
                unknown_0x34 = header.unknown_0x34;

                binary::NameTableHeader nameTableHeader;
                f.seekg(base + header.name_table_offset);
                f.read(reinterpret_cast<char*>(&nameTableHeader), sizeof(binary::NameTableHeader));

                std::vector<uint64_t> bone_name_offsets(nameTableHeader.bone_name_count);
                f.seekg(base + nameTableHeader.bone_name_offsets_offset);
                f.read(reinterpret_cast<char*>(bone_name_offsets.data()), sizeof(uint64_t) * nameTableHeader.bone_name_count);

                std::vector<std::string> bone_names(nameTableHeader.bone_name_count);
                for (int i = 0; i < nameTableHeader.bone_name_count; i++) {
                    f.seekg(base + bone_name_offsets[i] + header.strings_offset);
                    std::string result;
                    char ch;
                    while (f.get(ch)) {
                        if (ch == '\0') break;
                        result += ch;
                    }
                    bone_names.push_back(result);
                }

                std::vector<uint64_t> material_name_offsets(nameTableHeader.material_name_count);
                f.seekg(base + nameTableHeader.material_name_offsets_offset);
                f.read(reinterpret_cast<char*>(material_name_offsets.data()), sizeof(uint64_t) * nameTableHeader.material_name_count);

                std::vector<std::string> material_names(nameTableHeader.material_name_count);
                for (int i = 0; i < nameTableHeader.material_name_count; i++) {
                    f.seekg(base + material_name_offsets[i] + header.strings_offset);
                    std::string result;
                    char ch;
                    while (f.get(ch)) {
                        if (ch == '\0') break;
                        result += ch;
                    }
                    material_names.push_back(result);
                }

                skeleton.read(f, header.skeleton_offset, base);

                f.seekg(base + header.ibpm_offset);
                std::vector<binary::Ibpm> ibpms(header.ibpm_count);
                f.read(reinterpret_cast<char*>(ibpms.data()), sizeof(binary::Ibpm) * header.ibpm_count);

                std::unordered_map<uint32_t, std::string> hash_to_bone_name;
                for (const auto& name : bone_names) {
                    uint32_t hash = crc32((const uint8_t*)name.c_str(), name.length());
                    hash_to_bone_name[hash] = name;
                }

                std::unordered_map<std::string, std::shared_ptr<Bone>> name_to_bone;
                for (auto& bone : skeleton.bones) {
                    auto it = hash_to_bone_name.find(bone->name_hash);
                    if (it != hash_to_bone_name.end()) {
                        bone->name = it->second;
                        name_to_bone[bone->name] = bone;
                    }
                }

                //sanity tests
                assert(ibpms.size() == skeleton.bones.size());
                for (int i = 0; i < skeleton.bones.size(); i++) {
                    if (skeleton.bones[i]->name.rfind("ef_") == 0) {
                        assert(ibpmEqual(ibpms[i],binary::Ibpm()));
                        skeleton.bones[i]->is_effect = true;
                    } else {
                        assert(ibpmEqual(ibpms[i], ibpmFromMatrix(ComputeInverseBindPose(skeleton.bones[i]))));
                        skeleton.bones[i]->transform_actual = DecomposeMatrix(MatrixFromIbpm(ibpms[i]).inverse());
                    }
                }

                for (int i = 0; i < skeleton.bones.size(); i++) {
                    auto b = *skeleton.bones[i].get();
                    if (!b.is_effect) {
                        assert(transformEqual(b.transform_actual,getAbsoluteTransform(b.transform, b.parent ? &b.parent->transform_actual : nullptr)));
                        assert(transformEqual(b.transform,getRelativeTransform(b.transform_actual, b.parent ? &b.parent->transform_actual : nullptr)));
                    }
                }

                std::vector<binary::MeshHeader> meshHeaders(header.mesh_count);
                f.seekg(base + header.mesh_offset);
                f.read(reinterpret_cast<char*>(meshHeaders.data()), sizeof(binary::MeshHeader) * header.mesh_count);
                
                for(int i = 0; i < header.mesh_count; i++) {
                    Mesh mesh;

                    mesh.unknown_0x18 = meshHeaders[i].unknown_0x18;
                    mesh.unknown_0x4C = meshHeaders[i].unknown_0x4C;
                    mesh.unknown_0x50 = meshHeaders[i].unknown_0x50;

                    mesh.flag_0 = meshHeaders[i].flags.flag_0;
                    mesh.flag_1 = meshHeaders[i].flags.flag_1;
                    mesh.flag_2 = meshHeaders[i].flags.flag_2;
                    mesh.flag_3 = meshHeaders[i].flags.flag_3;
                    mesh.flag_4 = meshHeaders[i].flags.flag_4;
                    mesh.flag_5 = meshHeaders[i].flags.flag_5;
                    mesh.flag_6 = meshHeaders[i].flags.flag_6;
                    mesh.flag_7 = meshHeaders[i].flags.flag_7;

                    std::vector<uint32_t> matrix_palette(meshHeaders[i].matrix_palette_count);
                    f.seekg(base + meshHeaders[i].matrix_palette_offset);
                    f.read(reinterpret_cast<char*>(matrix_palette.data()), sizeof(uint32_t) * meshHeaders[i].matrix_palette_count);

                    for(int y = 0; y < matrix_palette.size(); y++) {
                        mesh.matrix_palette.push_back(skeleton.bones[matrix_palette[y]]);
                    }

                    mesh.name_hash = meshHeaders[i].name_hash;
                    if (mesh.name_hash != 0 && meshHeaders[i].name_offset != 0) {
                        f.seekg(base + header.strings_offset + meshHeaders[i].name_offset);

                        std::string result;
                        char ch;
                        while (f.get(ch)) {
                            if (ch == '\0') break;
                            result += ch;
                        }
                        mesh.name = result;

                        assert(mesh.name_hash == crc32((const uint8_t*)mesh.name.data(),mesh.name.size()));
                    }

                    std::vector<binary::MeshAttribute> meshAttributes(meshHeaders[i].attribute_count);
                    f.seekg(base + meshHeaders[i].attributes_offset);
                    f.read(reinterpret_cast<char*>(meshAttributes.data()), sizeof(binary::MeshAttribute) * meshHeaders[i].attribute_count);

                    {
                        const size_t vpv = meshHeaders[i].bytes_per_vertex;
                        const size_t count = meshHeaders[i].vertex_count;

                        // total bytes of the entire vertex block:
                        size_t totalBytes = vpv * count;

                        // read everything in one shot
                        std::vector<uint8_t> allVertices(totalBytes);

                        f.seekg(base + meshHeaders[i].vertices_offset);
                        f.read((char*)allVertices.data(), totalBytes);

                        // validate
                        if (!f) throw std::runtime_error("Failed to read vertex buffer");

                        for (size_t y = 0; y < count; y++) {
                            const uint8_t* vptr = allVertices.data() + vpv * y;
                            mesh.vertices.emplace_back(vptr, meshAttributes);
                        }
                    }

                    mesh.indices.resize(meshHeaders[i].index_count);
                    f.seekg(base + meshHeaders[i].indices_offset);
                    f.read(reinterpret_cast<char*>(mesh.indices.data()), sizeof(uint16_t) * meshHeaders[i].index_count);

                    //bounding sanity check
                    std::vector<std::array<float, 3>> pos;
                    for (int i = 0; i < mesh.vertices.size() ; i++){
                        auto floats = std::get<std::vector<float>>(mesh.vertices[i].position.data);
                        pos.push_back(std::array<float, 3>{floats[0],floats[1],floats[2]});
                    }
                    BoundingInfo info = calculateBoundingInfo<float>(pos);

                    assert(boundingEquals(info,boundingFromHeader(meshHeaders[i])));

                    meshes.push_back(mesh);
                }
            }
    };
}
