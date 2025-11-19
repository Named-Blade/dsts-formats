#pragma once

#include <iostream>
#include <cstddef> 
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>

#include "../utils/hash.cpp"
#include "../utils/float16.cpp"
#include "skeleton.cpp"
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

            VertexAttribute(std::string name, size_t count = 1) {
                if (name == "float16") {
                    data = std::vector<float16>(count);
                }
                if (name == "float" || name == "float32") {
                    data = std::vector<float>(count);
                }
                if (name == "int8") {
                    data = std::vector<int8_t>(count);
                }
                if (name == "uint8") {
                    data = std::vector<uint8_t>(count);
                }
                if (name == "int16") {
                    data = std::vector<int16_t>(count);
                }
                if (name == "uint16") {
                    data = std::vector<uint16_t>(count);
                }
                if (name == "int32") {
                    data = std::vector<int32_t>(count);
                }
                if (name == "uint32") {
                    data = std::vector<uint32_t>(count);
                }
            }

            VertexAttribute(const binary::MeshAttribute& desc,
                            const uint8_t* basePtr)
            {
                const uint8_t* ptr = basePtr + desc.offset;

                switch (desc.dtype) {
                    case binary::Dtype::uByte:   data = loadVecMem<uint8_t>(ptr, desc.count); break;
                    case binary::Dtype::sByte:   data = loadVecMem<int8_t>(ptr, desc.count); break;
                    case binary::Dtype::uShort:  data = loadVecMem<uint16_t>(ptr, desc.count); break;
                    case binary::Dtype::sShort:  data = loadVecMem<int16_t>(ptr, desc.count); break;
                    case binary::Dtype::uInt:    data = loadVecMem<uint32_t>(ptr, desc.count); break;
                    case binary::Dtype::sInt:    data = loadVecMem<int32_t>(ptr, desc.count); break;
                    case binary::Dtype::Float:
                    case binary::Dtype::Float_alias:
                                                data = loadVecMem<float>(ptr, desc.count); break;
                    case binary::Dtype::Float16:
                    case binary::Dtype::Float16_alias:
                                                data = loadVecMem<float16>(ptr, desc.count); break;
                    default: assert(false);
                }
            }

            bool isEmpty() const { return std::holds_alternative<std::monostate>(data); }

        private:
            template<typename T>
            std::vector<T> loadVecMem(const uint8_t* ptr, size_t count) {
                std::vector<T> out(count);
                memcpy(out.data(), ptr, count * sizeof(T));
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

            Vertex() = default;

            Vertex(const uint8_t* vertexBuffer,
                const std::vector<binary::MeshAttribute>& attributes)
            {
                for (const auto& desc : attributes) {
                    switch (desc.atype) {
                    case Atype::Position: position = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::Normal:   normal   = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::Tangent:  tangent  = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::Binormal: binormal = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::UV1:      uv1      = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::UV2:      uv2      = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::UV3:      uv3      = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::unk_8:    unk_8    = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::Color:    color    = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::Index:    index    = VertexAttribute(desc, vertexBuffer); break;
                    case Atype::Weight:   weight   = VertexAttribute(desc, vertexBuffer); break;
                    default: assert(false);
                    }
                }
            }
    };

    struct Triangle {
        size_t i0,i1,i2;
        Vertex v0, v1, v2;
    };

    
    class Mesh{
        public:
            uint64_t unknown_0x18;
            uint32_t unknown_0x4C;
            uint32_t unknown_0x50;

            uint32_t name_hash;
            std::string name;

            std::vector<Vertex> vertices;
            std::vector<uint16_t> indices;
            PrimitiveType primitive;

            std::vector<std::shared_ptr<Bone>> matrix_palette;

            bool flag_0;
            bool flag_1;
            bool flag_2;
            bool flag_3;
            bool flag_4;
            bool flag_5;
            bool flag_6;
            bool flag_7;

            std::vector<Triangle> toTriangleListFromTriangles() {
                std::vector<Triangle> tris;
                tris.reserve(indices.size() / 3);

                for (size_t i = 0; i + 2 < indices.size(); i += 3)
                {
                    uint16_t i0 = indices[i + 0];
                    uint16_t i1 = indices[i + 1];
                    uint16_t i2 = indices[i + 2];

                    tris.push_back({
                        i0,
                        i1,
                        i2,
                        vertices[i0],
                        vertices[i1],
                        vertices[i2]
                    });
                }

                return tris;
            }

            std::vector<Triangle> toTriangleListFromTriangleStrip(){
                std::vector<Triangle> tris;
                tris.reserve(indices.size());

                bool flip = false;

                for (size_t i = 0; i + 2 < indices.size(); ++i)
                {
                    uint16_t i0 = indices[i + 0];
                    uint16_t i1 = indices[i + 1];
                    uint16_t i2 = indices[i + 2];

                    // skip degenerate triangles
                    if (i0 == i1 || i1 == i2 || i0 == i2){
                        flip = !flip;
                        continue;
                    }

                    if (!flip) {
                        // even tri: use natural order
                        tris.push_back({
                            i0,
                            i1,
                            i2,
                            vertices[i0],
                            vertices[i1],
                            vertices[i2]
                        });
                    } else {
                        // odd tri: flip winding
                        tris.push_back({
                            i1,
                            i0,
                            i2,
                            vertices[i1],
                            vertices[i0],
                            vertices[i2]
                        });
                    }

                    flip = !flip;
                }

                return tris;
            }

            std::vector<Triangle> toTriangleList(){
                if (primitive == PrimitiveType::Triangles) {
                    return toTriangleListFromTriangles();
                } else {
                    return toTriangleListFromTriangleStrip();
                }
            }

            void setName(std::string newName){
                name = newName;
                name_hash = crc32((const uint8_t*)name.data(),name.length());
            }
    };

    struct BoundingInfo {
        std::array<float ,3> centre;
        std::array<float ,3> bbox;
        float bounding_sphere_radius;
    };

    BoundingInfo boundingFromHeader(binary::MeshHeader &m) {
        BoundingInfo info;
        info.centre = std::array<float ,3>{m.centre[0],m.centre[1],m.centre[2]};
        info.bbox = std::array<float ,3>{m.bbox[0],m.bbox[1],m.bbox[2]};
        info.bounding_sphere_radius = m.bounding_sphere_radius;
        return info;
    }

    bool boundingEquals(BoundingInfo a, BoundingInfo b, float eps = 1e-4f) {
        if (std::fabs(a.bounding_sphere_radius - b.bounding_sphere_radius) > eps) {
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(a.centre[i] - b.centre[i]) > eps) {
                return false;
            }
        }
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(a.bbox[i] - b.bbox[i]) > eps) {
                return false;
            }
        }
        return true;
    }

    template<typename T>
	BoundingInfo calculateBoundingInfo(const std::vector<std::array<T, 3>>& positions) {
        BoundingInfo info{};

        if (positions.empty()) {
            return info; // returns zeros
        }

        std::array<T, 3> min_corner = positions[0];
        std::array<T, 3> max_corner = positions[0];

        // Compute min and max
        for (const auto& p : positions) {
            for (int i = 0; i < 3; ++i) {
                min_corner[i] = std::min(min_corner[i], p[i]);
                max_corner[i] = std::max(max_corner[i], p[i]);
            }
        }

        // Centre = (min + max) / 2
        for (int i = 0; i < 3; ++i) {
            info.centre[i] = (min_corner[i] + max_corner[i]) * 0.5;
            info.bbox[i]   = (max_corner[i] - min_corner[i]) * 0.5; // half extents
        }

        // Bounding sphere radius: max distance from centre
        T max_dist = 0.0;
        for (const auto& p : positions) {
            T dx = p[0] - info.centre[0];
            T dy = p[1] - info.centre[1];
            T dz = p[2] - info.centre[2];
            T dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            max_dist = std::max(max_dist, dist);
        }

        info.bounding_sphere_radius = max_dist;
        return info;
    }
}
