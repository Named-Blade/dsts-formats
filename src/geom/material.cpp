#pragma once

#include <bit>
#include <cstdint>
#include <variant>

#include "../utils/hash.cpp"
#include "binary/material.hpp"

#include "shader_params_bin.h"

namespace dsts::geom
{
    constexpr auto shaderParamDefs = std::bit_cast<std::array<binary::ShaderParamDef, binary::shaderParamNum>>(shader_params_bin);
    static const std::unordered_map<uint32_t, std::string_view> shaderParamMap = [] {
        std::unordered_map<uint32_t, std::string_view> m;
        m.reserve(shaderParamDefs.size());
        for (auto &p : shaderParamDefs)
            m.emplace(p.id, p.name);
        return m;
    }();

    class ShaderUniform {
        public:
            std::string parameter_name;
            uint16_t parameter_id = 0;

            std::variant<std::vector<float>, std::string> value;

            //unks
            uint32_t unknown_0xC;
            uint32_t unknown_0x14;
            uint32_t unknown_0x18;
            uint32_t unknown_0x1C;

            ShaderUniform() = default;

            const std::string& getParameterName() const { return parameter_name; }
            uint16_t getParameterId() const { return parameter_id; }

            void setParameterId(uint16_t id) {
                auto it = shaderParamMap.find(id);
                if (it == shaderParamMap.end()) {
                    throw std::runtime_error("Invalid parameter_id: no matching name");
                }
                parameter_id = id;
                parameter_name = std::string(it->second);
            }

            void setParameterName(const std::string& name) {
                auto it = std::find_if(
                    shaderParamMap.begin(), shaderParamMap.end(),
                    [&](auto& p){ return p.second == name; });

                if (it == shaderParamMap.end()) {
                    throw std::runtime_error("Invalid parameter_name: no matching id");
                }
                parameter_name = name;
                parameter_id = it->first;
            } 

            std::string uniformType() {
                return std::visit([](auto &&arg) -> std::string {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::string>)
                        return "texture";
                    else
                        return "float";
                }, value);
            }
    };

    class ShaderSetting {
        public:
            std::string parameter_name;
            uint16_t parameter_id = 0;

            //unks
            binary::Payload payload;
            uint16_t unknown_0x12;
            uint32_t unknown_0x14;
            uint32_t unknown_0x18;
            uint32_t unknown_0x1C;

            ShaderSetting() = default;

            const std::string& getParameterName() const { return parameter_name; }
            uint16_t getParameterId() const { return parameter_id; }

            void setParameterId(uint16_t id) {
                auto it = shaderParamMap.find(id);
                if (it == shaderParamMap.end()) {
                    throw std::runtime_error("Invalid parameter_id: no matching name");
                }
                parameter_id = id;
                parameter_name = std::string(it->second);
            }

            void setParameterName(const std::string& name) {
                auto it = std::find_if(
                    shaderParamMap.begin(), shaderParamMap.end(),
                    [&](auto& p){ return p.second == name; });

                if (it == shaderParamMap.end()) {
                    throw std::runtime_error("Invalid parameter_name: no matching id");
                }
                parameter_name = name;
                parameter_id = it->first;
            } 
    };

    class Shader {
        public:
            static constexpr std::size_t NameSize = 14;
            std::array<uint32_t, NameSize> name_data{};

            Shader() = default;

            std::string getShaderName() const {
                std::string name;

                for (std::size_t i = 0; i < NameSize; ++i) {
                    if (i != 0 && i % 3 == 0) {
                        name += "_";
                    }
                    name += encode(name_data[i]);
                }

                return name;
            }

            void setShaderName(const std::string& encodedName) {
                static const std::string alphabet = "0123456789abcdefghijklmnopqrstuvwxyz#$[]{}+@=&";

                // Remove underscores
                std::string cleanName = encodedName;
                cleanName.erase(std::remove(cleanName.begin(), cleanName.end(), '_'), cleanName.end());

                // Check length
                if (cleanName.length() != NameSize * 6) {
                    throw std::runtime_error("Encoded name has invalid length");
                }

                // Check allowed characters
                for (char c : cleanName) {
                    if (alphabet.find(c) == std::string::npos) {
                        throw std::runtime_error(std::string("Invalid character in encoded name: ") + c);
                    }
                }

                // Decode segments
                for (std::size_t i = 0; i < NameSize; ++i) {
                    std::string segment = cleanName.substr(i * 6, 6);

                    // Decode to uint64_t first to safely check overflow
                    uint64_t value = 0;
                    for (char c : segment) {
                        value *= 46;
                        size_t index = alphabet.find(c);
                        value += static_cast<uint64_t>(index);
                    }

                    // Check if it fits in uint32_t
                    if (value > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
                        throw std::runtime_error("Segment value exceeds uint32_t range");
                    }

                    name_data[i] = static_cast<uint32_t>(value);
                }
            }

        private:
            static std::string encode(uint32_t n) {
                static const std::string alphabet =
                    "0123456789abcdefghijklmnopqrstuvwxyz#$[]{}+@=&";

                std::string encoded;

                uint32_t value = n;

                // Base-46 conversion
                if (value == 0) {
                    encoded = "0";
                } else {
                    while (value > 0) {
                        uint32_t digit = value % 46;
                        value /= 46;
                        encoded.insert(encoded.begin(), alphabet[digit]);
                    }
                }

                // Pad to 6 characters
                if (encoded.length() < 6) {
                    encoded.insert(0, 6 - encoded.length(), alphabet[0]);
                }

                return encoded;
            }
            static uint32_t decode(const std::string& str) {
                static const std::string alphabet =
                    "0123456789abcdefghijklmnopqrstuvwxyz#$[]{}+@=&";

                uint32_t value = 0;
                for (char c : str) {
                    value *= 46;
                    size_t index = alphabet.find(c);
                    if (index != std::string::npos) {
                        value += static_cast<uint32_t>(index);
                    } else {
                        throw std::runtime_error("Invalid character in encoded name");
                    }
                }
                return value;
            }
    };

    class Material {
        public:
            uint32_t unknown_0x314;
            uint32_t unknown_0x318;
            uint32_t unknown_0x31C;
            uint16_t unknown_0x324;
            uint16_t unknown_0x326;

            uint32_t name_hash;
            std::string name;

            std::array<Shader, 14> shaders;
            std::vector<ShaderUniform> uniforms;
            std::vector<ShaderSetting> settings;

            void setName(std::string newName){
                name = newName;
                name_hash = crc32((const uint8_t*)name.data(),name.length());
            }
    };
}