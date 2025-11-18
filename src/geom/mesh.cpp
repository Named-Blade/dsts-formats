#pragma once

#include <iostream>
#include <cstddef> 
#include <vector>
#include <array>
#include <string>
#include <memory>

#include "../utils/hash.cpp"
#include "../utils/float16.cpp"
#include "binary/mesh.hpp"

namespace dsts::geom
{
    using AttributeData = std::variant<
        std::monostate,
        std::vector<uint8_t>,
        std::vector<int8_t>,
        std::vector<uint16_t>,
        std::vector<int16_t>,
        std::vector<uint32_t>,
        std::vector<int32_t>,
        std::vector<float>,
        std::vector<float16>
    >;

    class VertexAttribute {
        public:
            AttributeData data;

            VertexAttribute() = default;

            VertexAttribute(const binary::MeshAttribute& desc,
                            std::istream& stream,
                            std::streampos baseOffset)
            {
                // Seek to attribute position
                stream.seekg(baseOffset + static_cast<std::streamoff>(desc.offset),
                            std::ios::beg);

                if (!stream.good()) {
                    data = std::monostate{};
                    return;
                }

                switch (desc.dtype) {
                    case binary::Dtype::uByte:   data = loadVec<uint8_t>(stream, desc.count); break;
                    case binary::Dtype::sByte:   data = loadVec<int8_t>(stream, desc.count); break;
                    case binary::Dtype::uShort:  data = loadVec<uint16_t>(stream, desc.count); break;
                    case binary::Dtype::sShort:  data = loadVec<int16_t>(stream, desc.count); break;
                    case binary::Dtype::uInt:    data = loadVec<uint32_t>(stream, desc.count); break;
                    case binary::Dtype::sInt:    data = loadVec<int32_t>(stream, desc.count); break;

                    case binary::Dtype::Float:
                    case binary::Dtype::Float_alias:
                        data = loadVec<float>(stream, desc.count);
                        break;

                    case binary::Dtype::Float16:
                    case binary::Dtype::Float16_alias:
                        data = loadVec<float16>(stream, desc.count);
                        break;

                    default: assert(false);
                }
            }

            bool isEmpty() const { return std::holds_alternative<std::monostate>(data); }

        private:
            template<typename T>
            std::vector<T> loadVec(std::istream& stream, size_t count) {
                std::vector<T> out(count);
                stream.read(reinterpret_cast<char*>(out.data()),
                            static_cast<std::streamsize>(count * sizeof(T)));
                return out;
            }
    };

    class Vertex {
        public:
            VertexAttribute position;
            VertexAttribute normal;
            VertexAttribute tangent;
            VertexAttribute binormal;
            VertexAttribute uv1;
            VertexAttribute uv2;
            VertexAttribute uv3;
            VertexAttribute unk_8;
            VertexAttribute color;
            VertexAttribute index;
            VertexAttribute weight;

            Vertex(std::istream& stream,
                std::streampos baseOffset,
                const std::vector<binary::MeshAttribute>& attributes)
            {
                for (const auto& desc : attributes) {
                    switch (desc.atype) {
                    case Atype::Position: position = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::Normal:   normal   = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::Tangent:  tangent  = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::Binormal: binormal = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::UV1:      uv1      = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::UV2:      uv2      = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::UV3:      uv3      = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::unk_8:    unk_8    = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::Color:    color    = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::Index:    index    = VertexAttribute(desc, stream, baseOffset); break;
                    case Atype::Weight:   weight   = VertexAttribute(desc, stream, baseOffset); break;
                    default: assert(false);
                    }
                }
            }
    };

    
    class Mesh{
        public:
            uint64_t unknown_0x18;
            uint32_t unknown_0x4C;
            uint32_t unknown_0x50;

            uint32_t name_hash;
            std::string name;

            std::vector<Vertex> vertices;

            void setName(std::string newName){
                name = newName;
                name_hash = crc32((const uint8_t*)name.data(),name.length());
            }
    };
}
