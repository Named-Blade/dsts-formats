#pragma once

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
    };

    class Material {
        public:
            std::array<Shader, 14> shaders;
    };
}