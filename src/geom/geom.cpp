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
#include "material.cpp"
#include "stringSection.cpp"
#include "../utils/hash.cpp"
#include "../utils/stream.cpp"
#include "../utils/align.cpp"

namespace dsts::geom
{  
    class Geom {
        public:
            uint32_t unknown_0x10 = 0;
            uint32_t unknown_0x30 = 0;
            uint32_t unknown_0x34 = 0;

            binary::Clut clut;

            Skeleton skeleton;
            std::vector<Mesh> meshes;
            std::vector<std::shared_ptr<Material>> materials;

            void read(std::istream& f, int base = 0){
                binary::GeomHeader header;

                f.seekg(base);
                f.read(reinterpret_cast<char*>(&header), sizeof(binary::GeomHeader));

                assert(header.version == 316);

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

                for (auto& bone : skeleton.bones) {
                    auto it = hash_to_bone_name.find(bone->name_hash);
                    if (it != hash_to_bone_name.end()) {
                        bone->name = it->second;
                    }
                }

                //sanity tests
                assert(ibpms.size() == skeleton.bones.size());
                for (int i = 0; i < skeleton.bones.size(); i++) {
                    assert(ibpmEqual(ibpms[i], ibpmFromMatrix(ComputeInverseBindPose(skeleton.bones[i]))));
                    skeleton.bones[i]->transform_actual = DecomposeMatrix(MatrixFromIbpm(ibpms[i]).inverse());
                }

                for (int i = 0; i < skeleton.bones.size(); i++) {
                    auto b = *skeleton.bones[i].get();
                    assert(transformEqual(b.transform_actual,getAbsoluteTransform(b.transform, b.parent ? &b.parent->transform_actual : nullptr)));
                    assert(transformEqual(b.transform,getRelativeTransform(b.transform_actual, b.parent ? &b.parent->transform_actual : nullptr)));
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

                    if (meshHeaders[i].vertex_groups_per_vertex == 0) {
                        for (auto &v : mesh.vertices) {
                            v.index.data  = std::vector<uint8_t>{0};
                            v.weight.data = std::vector<float16>{(float16)1.0};
                        }   
                    }
                    else if (meshHeaders[i].vertex_groups_per_vertex == 1) {
                        for (auto &v : mesh.vertices) {
                            auto pos = std::get<std::vector<float>>(v.position.data);
                            v.index.data  = std::vector<uint8_t>{(uint8_t)pos[3]};
                            v.weight.data = std::vector<float16>{(float16)1.0};
                        }   
                    }

                    mesh.indices.resize(meshHeaders[i].index_count);
                    f.seekg(base + meshHeaders[i].indices_offset);
                    f.read(reinterpret_cast<char*>(mesh.indices.data()), sizeof(uint16_t) * meshHeaders[i].index_count);

                    //bounding sanity check
                    std::vector<std::array<float, 3>> pos;
                    pos.reserve(mesh.vertices.size());
                    for (const auto& vertex : mesh.vertices) {
                        const auto& floats = std::get<std::vector<float>>(vertex.position.data);
                        pos.emplace_back(std::array<float, 3>{floats[0], floats[1], floats[2]});
                    }
                    BoundingInfo info = calculateBoundingInfo<float>(pos);

                    assert(boundingEquals(info,boundingFromHeader(meshHeaders[i])));

                    assert(meshHeaders[i].controller_offset == 0);

                    meshes.push_back(mesh);
                }

                {
                    std::vector<std::array<float, 3>> pos_list;
                    for (const auto &mesh : meshes) {
                        for (const auto &vertex : mesh.vertices){
                            const auto& floats = std::get<std::vector<float>>(vertex.position.data);
                            pos_list.emplace_back(std::array<float, 3>{floats[0], floats[1], floats[2]});
                        }
                    }
                    BoundingInfo info = calculateBoundingInfo<float>(pos_list);
                    info.bounding_sphere_radius = 0.0;

                    BoundingInfo h_info{};
                    h_info.bbox = {header.bbox[0],header.bbox[1],header.bbox[2]};
                    h_info.centre = {header.centre[0],header.centre[1],header.centre[2]};

                    assert(boundingEquals(info,h_info));
                }

                f.seekg(base + header.material_offset);
                for (int i = 0; i < header.material_count ; i++){
                    std::shared_ptr<Material> mat_ptr = std::make_shared<Material>();
                    Material &material = *mat_ptr.get();

                    binary::MaterialHeader materialHeader;
                    f.read(reinterpret_cast<char*>(&materialHeader), sizeof(materialHeader));

                    material.name_hash = materialHeader.name_hash;

                    material.unknown_0x314 = materialHeader.unknown_0x314;
                    material.unknown_0x318 = materialHeader.unknown_0x318;
                    material.unknown_0x31C = materialHeader.unknown_0x31C;
                    material.unknown_0x324 = materialHeader.unknown_0x324;
                    material.unknown_0x326 = materialHeader.unknown_0x326;

                    for (int y = 0; y < 14; y++) {
                        std::copy(std::begin(materialHeader.shaders[y].name_data), std::end(materialHeader.shaders[y].name_data), material.shaders[y].name_data.begin());
                    }

                    std::vector<binary::ShaderUniform> shaderUniforms(materialHeader.uniform_count);
                    std::vector<binary::ShaderSetting> shaderSettings(materialHeader.setting_count);

                    f.read(reinterpret_cast<char*>(shaderUniforms.data()), sizeof(binary::ShaderUniform) * materialHeader.uniform_count);
                    f.read(reinterpret_cast<char*>(shaderSettings.data()), sizeof(binary::ShaderSetting) * materialHeader.setting_count);

                    for (const auto &uniform_bin : shaderUniforms) {
                        ShaderUniform uniform;

                        uniform.parameter_id = uniform_bin.parameter_id;
                        uniform.parameter_name = shaderParamMap.at(uniform.parameter_id);

                        uniform.unknown_0x14 = uniform_bin.unknown_0x14;
                        uniform.unknown_0x18 = uniform_bin.unknown_0x18;
                        uniform.unknown_0x1C = uniform_bin.unknown_0x1C;

                        if (uniform_bin.float_count > 0) {

                            std::vector<float> vec(uniform_bin.float_count);
                            for (int y = 0; y < uniform_bin.float_count ; y++){
                                vec[y]=(uniform_bin.payload.floats.floats[y]);
                            }
                            uniform.value = vec;
                            
                        } else {
                            size_t current = f.tellg();

                            f.seekg(base + header.strings_offset + uniform_bin.payload.texture.texture_name_offset);

                            std::string result;
                            char ch;
                            while (f.get(ch)) {
                                if (ch == '\0') break;
                                result += ch;
                            }
                            assert(result.size() == uniform_bin.payload.texture.texture_name_length);
                            uniform.value = result;

                            uniform.unknown_0xC = uniform_bin.payload.texture.unknown_0xC;

                            f.seekg(current);
                        }

                        material.uniforms.push_back(uniform);
                    }

                    for (const auto &setting_bin : shaderSettings) {
                        ShaderSetting setting;

                        setting.parameter_id = setting_bin.parameter_id;
                        setting.parameter_name = shaderParamMap.at(setting.parameter_id);

                        setting.payload = setting_bin.payload;

                        setting.unknown_0x12 = setting_bin.unknown_0x12;
                        setting.unknown_0x14 = setting_bin.unknown_0x14;
                        setting.unknown_0x18 = setting_bin.unknown_0x18;
                        setting.unknown_0x1C = setting_bin.unknown_0x1C;

                        material.settings.push_back(setting);
                    }

                    materials.push_back(std::move(mat_ptr));
                }

                std::unordered_map<uint32_t, std::string> hash_to_material_name;
                for (const auto& name : material_names) {
                    uint32_t hash = crc32((const uint8_t*)name.c_str(), name.length());
                    hash_to_material_name[hash] = name;
                }

                for (auto& material : materials) {
                    auto it = hash_to_material_name.find(material->name_hash);
                    if (it != hash_to_material_name.end()) {
                        material->name = it->second;
                    }
                }

                for (int i = 0; i < meshes.size() ; i++) {
                    meshes[i].material = materials[meshHeaders[i].material_idx];
                }

                //do this properly latter
                f.seekg(base + header.clut_offset);
                f.read(reinterpret_cast<char*>(&clut), sizeof(binary::Clut));

                assert(header.light_count == 0);
                assert(header.camera_count == 0);
            }

            void write(std::ostream& f, int base = 0){
                binary::GeomHeader header;

                header.unknown_0x10 = unknown_0x10;
                header.unknown_0x30 = unknown_0x30;
                header.unknown_0x34 = unknown_0x34;

                header.mesh_count = meshes.size();
                header.material_count = materials.size();
                header.ibpm_count = skeleton.bones.size();

                binary::NameTableHeader nameTable;
                size_t nameTableBase = sizeof(binary::GeomHeader);
                header.name_table_offset = nameTableBase;
                std::vector<uint64_t> bone_name_offsets(skeleton.bones.size());
                std::vector<uint64_t> material_name_offsets(materials.size());
                StringSection stringSection{};

                nameTable.bone_name_count = skeleton.bones.size();
                nameTable.material_name_count = materials.size();
                nameTable.bone_name_offsets_offset = nameTableBase + sizeof(binary::NameTableHeader);
                nameTable.material_name_offsets_offset = nameTable.bone_name_offsets_offset + sizeof(uint64_t) * nameTable.bone_name_count;

                std::vector<binary::MeshHeader> meshHeaders(meshes.size());
                std::vector<std::string> vertices(meshes.size());
                std::vector<std::vector<uint32_t>> matrixPalettes(meshes.size());
                std::vector<std::vector<uint16_t>> indices(meshes.size());
                std::vector<std::vector<binary::MeshAttribute>> attributes(meshes.size());
                size_t meshesBase = nameTable.material_name_offsets_offset + sizeof(uint64_t) * nameTable.material_name_count;
                size_t meshDataBase = meshesBase + sizeof(binary::MeshHeader) * meshes.size();
                size_t meshDataSize{};
                header.mesh_offset = meshesBase;

                std::vector<std::array<float, 3>> pos_all;
                size_t totalVertexCount = 0;
                for (const auto& mesh : meshes) {
                    totalVertexCount += mesh.vertices.size();
                }
                pos_all.reserve(totalVertexCount);

                for (int i = 0; i < meshes.size(); i++) {
                    auto mesh = meshes[i];
                    binary::MeshHeader meshHeader{};

                    meshHeader.name_hash = mesh.name_hash;
                    meshHeader.name_offset = stringSection.add(mesh.name);

                    meshHeader.material_idx = getIndex(materials, mesh.material);

                    meshHeader.unknown_0x18 = mesh.unknown_0x18;
                    meshHeader.unknown_0x4C = mesh.unknown_0x4C;
                    meshHeader.unknown_0x50 = mesh.unknown_0x50;

                    auto attrData = buildMeshAttributes(mesh.vertices);
                    meshHeader.bytes_per_vertex = attrData.totalSizeBytes;

                    meshHeader.vertices_offset = meshDataBase + meshDataSize;
                    meshHeader.vertex_count = mesh.vertices.size();
                    std::string verts = packVertices(attrData.attributes, mesh.vertices, attrData.totalSizeBytes);
                    meshDataSize += verts.size();
                    vertices[i] = verts;

                    meshHeader.matrix_palette_offset = meshDataBase + meshDataSize;
                    meshHeader.matrix_palette_count = mesh.matrix_palette.size();
                    std::vector<uint32_t> palette(mesh.matrix_palette.size());
                    for (int y = 0; y < mesh.matrix_palette.size() ; y++) {
                        palette[y] = getIndex(skeleton.bones, mesh.matrix_palette[y]);
                    }
                    meshDataSize += sizeof(uint32_t) * palette.size();
                    matrixPalettes[i] = palette;

                    meshHeader.indices_offset = meshDataBase + meshDataSize;
                    meshHeader.index_count = mesh.indices.size();
                    meshDataSize += sizeof(uint16_t) * mesh.indices.size();
                    indices[i] = mesh.indices;

                    meshHeader.attributes_offset = meshDataBase + meshDataSize;
                    meshHeader.attribute_count = attrData.attributes.size();
                    meshDataSize += sizeof(binary::MeshAttribute) * attrData.attributes.size();
                    attributes[i] = attrData.attributes;

                    std::vector<std::array<float, 3>> pos;
                    pos.reserve(mesh.vertices.size());
                    for (const auto& vertex : mesh.vertices) {
                        const auto& floats = std::get<std::vector<float>>(vertex.position.data);
                        pos.emplace_back(std::array<float, 3>{floats[0], floats[1], floats[2]});
                        pos_all.emplace_back(std::array<float, 3>{floats[0], floats[1], floats[2]});
                    }
                    BoundingInfo info = calculateBoundingInfo<float>(pos);

                    meshHeader.bounding_sphere_radius = info.bounding_sphere_radius;
                    meshHeader.bbox[0] = info.bbox[0];
                    meshHeader.bbox[1] = info.bbox[1];
                    meshHeader.bbox[2] = info.bbox[2];
                    meshHeader.centre[0] = info.centre[0];
                    meshHeader.centre[1] = info.centre[1];
                    meshHeader.centre[2] = info.centre[2];
                    
                    meshHeaders[i] = meshHeader;
                }

                BoundingInfo info = calculateBoundingInfo<float>(pos_all);
                header.bbox[0] = info.bbox[0];
                header.bbox[1] = info.bbox[1];
                header.bbox[2] = info.bbox[2];
                header.centre[0] = info.centre[0];
                header.centre[1] = info.centre[1];
                header.centre[2] = info.centre[2];


                std::vector<binary::MaterialHeader> materialHeaders(materials.size());
                std::vector<std::vector<binary::ShaderUniform>> materialUniforms(materials.size());
                std::vector<std::vector<binary::ShaderSetting>> materialSettings(materials.size());
                std::vector<size_t> materialHeaderBases(materials.size());
                size_t materialsBase = meshDataBase + meshDataSize;
                header.material_offset = materialsBase;

                size_t lastMaterialEnd = materialsBase;
                for (int i = 0; i < materials.size(); i++) {
                    binary::MaterialHeader materialHeader{};

                    materialHeader.name_hash = materials[i]->name_hash;
                    material_name_offsets[i] = stringSection.add(materials[i]->name);

                    materialHeader.uniform_count = materials[i]->uniforms.size();
                    materialHeader.setting_count = materials[i]->settings.size();

                    materialHeader.unknown_0x314 = materials[i]->unknown_0x314;
                    materialHeader.unknown_0x318 = materials[i]->unknown_0x318;
                    materialHeader.unknown_0x31C = materials[i]->unknown_0x31C;
                    materialHeader.unknown_0x324 = materials[i]->unknown_0x324;
                    materialHeader.unknown_0x326 = materials[i]->unknown_0x326;

                    for (int y = 0; y < 14; y++) {
                        std::copy(
                            std::begin(materials[i]->shaders[y].name_data),
                            std::end(materials[i]->shaders[y].name_data),
                            std::begin(materialHeader.shaders[y].name_data)
                        );
                    }

                    std::vector<binary::ShaderUniform> uniforms(materials[i]->uniforms.size());
                    for (int y = 0; y < materials[i]->uniforms.size(); y++) {
                        binary::ShaderUniform uniform{};

                        if (materials[i]->uniforms[y].uniformType() == "float") {
                            binary::FloatPayload payload;
                            auto value = std::get<std::vector<float>>(materials[i]->uniforms[y].value);

                            std::copy(
                                value.begin(),
                                value.end(),
                                payload.floats
                            );

                            uniform.float_count = value.size();
                            uniform.payload.floats = payload;
                        } else {
                            auto value = std::get<std::string>(materials[i]->uniforms[y].value);
                            binary::TexturePayload payload;

                            payload.texture_name_offset = stringSection.add(value);
                            payload.texture_name_length = value.size();
                            payload.unknown_0xC = materials[i]->uniforms[y].unknown_0xC;

                            uniform.float_count = 0;
                            uniform.payload.texture = payload;
                        }

                        uniform.parameter_id = materials[i]->uniforms[y].parameter_id;

                        uniform.unknown_0x14 = materials[i]->uniforms[y].unknown_0x14;
                        uniform.unknown_0x18 = materials[i]->uniforms[y].unknown_0x18;
                        uniform.unknown_0x1C = materials[i]->uniforms[y].unknown_0x1C;

                        uniforms[y] = uniform;
                    }
                    materialUniforms[i] = uniforms;

                    std::vector<binary::ShaderSetting> settings(materials[i]->settings.size());
                    for (int y = 0; y < materials[i]->settings.size(); y++) {
                        binary::ShaderSetting setting{};

                        setting.payload = materials[i]->settings[y].payload;
                        setting.parameter_id = materials[i]->settings[y].parameter_id;

                        setting.unknown_0x12 = materials[i]->settings[y].unknown_0x12;
                        setting.unknown_0x14 = materials[i]->settings[y].unknown_0x14;
                        setting.unknown_0x18 = materials[i]->settings[y].unknown_0x18;
                        setting.unknown_0x1C = materials[i]->settings[y].unknown_0x1C;

                        settings[y] = setting;
                    }
                    materialSettings[i] = settings;

                    materialHeaders[i] = materialHeader;
                    materialHeaderBases[i] = lastMaterialEnd;
                    lastMaterialEnd += sizeof(binary::MaterialHeader)
                        + sizeof(binary::ShaderUniform) * materials[i]->uniforms.size()
                        + sizeof(binary::ShaderSetting) * materials[i]->settings.size();
                }

                std::vector<binary::Ibpm> ibpms(skeleton.bones.size());
                size_t ibpmsBase = align(lastMaterialEnd,0x10);
                header.ibpm_offset = ibpmsBase;

                for (int i = 0; i < skeleton.bones.size(); i++) {
                    ibpms[i] = ibpmFromMatrix(ComputeInverseBindPose(skeleton.bones[i]));
                }

                size_t clutBase = ibpmsBase + sizeof(binary::Ibpm) * ibpms.size();
                header.clut_offset = clutBase;

                size_t stringsBase = clutBase + sizeof(binary::Clut);
                header.strings_offset = stringsBase;

                for (int i = 0; i < skeleton.bones.size(); i++) {
                    bone_name_offsets[i] = stringSection.add(skeleton.bones[i]->name);
                }

                MemoryStream skeletonStream;
                header.skeleton_file_size = skeleton.write(skeletonStream);
                std::string skeletonBin = skeletonStream.str();
                size_t skeletonBase = align(stringsBase + stringSection.data().size(),0x10) + 0x10;
                header.skeleton_offset = skeletonBase;

                //write
                {
                    f.seekp(base);
                    f.write(reinterpret_cast<char*>(&header), sizeof(binary::GeomHeader));

                    f.seekp(nameTableBase);
                    f.write(reinterpret_cast<char*>(&nameTable), sizeof(binary::NameTableHeader));
                    f.write(reinterpret_cast<char*>(bone_name_offsets.data()), sizeof(uint64_t) * bone_name_offsets.size());
                    f.write(reinterpret_cast<char*>(material_name_offsets.data()), sizeof(uint64_t) * material_name_offsets.size());

                    f.seekp(meshesBase);
                    f.write(reinterpret_cast<char*>(meshHeaders.data()), sizeof(binary::MeshHeader) * meshHeaders.size());
                    for (int i = 0; i < meshHeaders.size() ; i++) {
                        f.write(reinterpret_cast<char*>(vertices[i].data()),vertices[i].size());
                        f.write(reinterpret_cast<char*>(matrixPalettes[i].data()), sizeof(uint32_t) * matrixPalettes[i].size());
                        f.write(reinterpret_cast<char*>(indices[i].data()), sizeof(uint16_t) * indices[i].size());
                        f.write(reinterpret_cast<char*>(attributes[i].data()), sizeof(binary::MeshAttribute) * attributes[i].size());
                    }

                    for (int i = 0; i < materialHeaders.size(); i++) {
                        f.seekp(materialHeaderBases[i]);
                        f.write(reinterpret_cast<char*>(&materialHeaders[i]), sizeof(binary::MaterialHeader));
                        f.write(reinterpret_cast<char*>(materialUniforms[i].data()), sizeof(binary::ShaderUniform) * materialUniforms[i].size());
                        f.write(reinterpret_cast<char*>(materialSettings[i].data()), sizeof(binary::ShaderSetting) * materialSettings[i].size());
                    }

                    f.seekp(ibpmsBase);
                    f.write(reinterpret_cast<char*>(ibpms.data()), sizeof(binary::Ibpm) * ibpms.size());

                    f.seekp(clutBase);
                    f.write(reinterpret_cast<char*>(&clut), sizeof(binary::Clut));

                    f.seekp(stringsBase);
                    f.write(stringSection.data().data(),stringSection.data().size());

                    f.seekp(skeletonBase);
                    f.write(skeletonBin.data(),skeletonBin.size());
                }
            }

    };
}
