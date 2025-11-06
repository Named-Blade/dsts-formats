#pragma once

#include <functional>

#include "binary/clut.hpp"
#include "binary/enums.hpp"
#include "binary/geom.hpp"
#include "binary/material.hpp"
#include "binary/mesh.hpp"
#include "binary/name_table.hpp"
#include "binary/skeleton.hpp"

#include "skeleton.cpp"
#include "../utils/hash.cpp"

namespace dsts::geom
{  
    class Geom {
        public:
            Skeleton skeleton;

            void read(std::istream& f, int base = 0){
                binary::GeomHeader header;

                f.seekg(base);
                f.read(reinterpret_cast<char*>(&header), sizeof(binary::GeomHeader));

                binary::NameTableHeader nameTableHeader;
                f.seekg(header.name_table_offset);
                f.read(reinterpret_cast<char*>(&nameTableHeader), sizeof(binary::NameTableHeader));

                std::vector<uint64_t> bone_name_offsets(nameTableHeader.bone_name_count);
                f.seekg(nameTableHeader.bone_name_offsets_offset);
                f.read(reinterpret_cast<char*>(bone_name_offsets.data()), sizeof(uint64_t) * nameTableHeader.bone_name_count);

                std::vector<std::string> bone_names(nameTableHeader.bone_name_count);
                for (int i = 0; i < nameTableHeader.bone_name_count; i++) {
                    f.seekg(bone_name_offsets[i] + header.strings_offset);
                    std::string result;
                    char ch;
                    while (f.get(ch)) {
                        if (ch == '\0') break;
                        result += ch;
                    }
                    bone_names.push_back(result);
                }

                std::vector<uint64_t> material_name_offsets(nameTableHeader.material_name_count);
                f.seekg(nameTableHeader.material_name_offsets_offset);
                f.read(reinterpret_cast<char*>(material_name_offsets.data()), sizeof(uint64_t) * nameTableHeader.material_name_count);

                std::vector<std::string> material_names(nameTableHeader.material_name_count);
                for (int i = 0; i < nameTableHeader.material_name_count; i++) {
                    f.seekg(material_name_offsets[i] + header.strings_offset);
                    std::string result;
                    char ch;
                    while (f.get(ch)) {
                        if (ch == '\0') break;
                        result += ch;
                    }
                    material_names.push_back(result);
                }

                skeleton.read(f, header.skeleton_offset, base);

                f.seekg(base+header.ibpm_offset);
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

                assert(ibpms.size() == skeleton.bones.size());
                for (int i = 0; i < skeleton.bones.size(); i++) {
                    skeleton.bones[i]->transform_actual = DecomposeMatrix(MatrixFromIbpm(ibpms[i]).inverse());
                }
                
            }
    };
}
