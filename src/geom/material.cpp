#pragma once

#include "../utils/hash.cpp"
#include "binary/material.hpp"

namespace dsts::geom
{
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
                std::string cleanName = encodedName;
                cleanName.erase(std::remove(cleanName.begin(), cleanName.end(), '_'), cleanName.end());

                for (std::size_t i = 0; i < NameSize; ++i) {
                    std::string segment = cleanName.substr(i * 6, 6);
                    name_data[i] = decode(segment);
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
            uint32_t name_hash;
            std::string name;

            std::array<Shader, 14> shaders;

            void setName(std::string newName){
                name = newName;
                name_hash = crc32((const uint8_t*)name.data(),name.length());
            }
    };
}