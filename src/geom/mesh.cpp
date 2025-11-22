#pragma once

#include <iostream>
#include <cstddef> 
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <optional>

#include "../utils/hash.cpp"
#include "../utils/float16.cpp"
#include "skeleton.cpp"
#include "material.cpp"
#include "binary/mesh.hpp"

namespace dsts::geom
{
    template <typename T, size_t Cap = 4>
    class InlineVec {
    public:
        InlineVec() = default;
        InlineVec(size_t count) : m_size(static_cast<uint8_t>(count)) { assert(count <= Cap); }

        InlineVec(std::initializer_list<T> init) {
            assert(init.size() <= Cap);
            m_size = static_cast<uint8_t>(init.size());
            std::copy(init.begin(), init.end(), m_data);
        }

        T* data() { return m_data; }
        const T* data() const { return m_data; }
        size_t size() const { return m_size; }
        void resize(size_t n) { assert(n <= Cap); m_size = static_cast<uint8_t>(n); }
        
        T& operator[](size_t i) { return m_data[i]; }
        const T& operator[](size_t i) const { return m_data[i]; }

    private:
        T m_data[Cap]{};
        uint8_t m_size = 0;
    };

    using AttributeData = std::variant<
        std::monostate,
        InlineVec<uint8_t>,
        InlineVec<int8_t>,
        InlineVec<uint16_t>,
        InlineVec<int16_t>,
        InlineVec<uint32_t>,
        InlineVec<int32_t>,
        InlineVec<float>,
        InlineVec<float16>
    >;

    class VertexAttribute {
    public:
        AttributeData data;

        VertexAttribute() = default;

        // Keep existing constructor interface
        VertexAttribute(std::string name, size_t count = 1) {
            if (name == "float" || name == "float32") data = InlineVec<float>(count);
            else if (name == "float16") data = InlineVec<float16>(count);
            else if (name == "int8")    data = InlineVec<int8_t>(count);
            else if (name == "uint8")   data = InlineVec<uint8_t>(count);
            else if (name == "int16")   data = InlineVec<int16_t>(count);
            else if (name == "uint16")  data = InlineVec<uint16_t>(count);
            else if (name == "int32")   data = InlineVec<int32_t>(count);
            else if (name == "uint32")  data = InlineVec<uint32_t>(count);
        }
        
        bool isEmpty() const { return std::holds_alternative<std::monostate>(data); }

        // Helper to directly write memory without reallocation
        template <typename T>
        void setFromPtr(const uint8_t* ptr, size_t count) {
            // Emplace construct the InlineVec variant
            data.emplace<InlineVec<T>>(count);
            // Get pointer to the data we just created
            auto& vec = std::get<InlineVec<T>>(data);
            memcpy(vec.data(), ptr, count * sizeof(T));
        }
    };

    class Vertex {
    public:
        // Interface remains exactly the same
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

        // Optimization: Removed the heavy parsing constructor.
        // Logic is moved to unpackVertices for O(1) setup cost per vertex.
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

            std::shared_ptr<Material> material;

            bool flag_0;
            bool flag_1;
            bool flag_2;
            bool flag_3;
            bool flag_4;
            bool flag_5;
            bool flag_6;
            bool flag_7;

            std::vector<uint32_t> get_indices_as_triangle_list() {
                // Case 1: It's already a Triangle List
                // If your internal 'indices' are uint16_t, we cast them to uint32_t for safety in Python/Blender
                // (Blender supports int loops, so 32-bit is safer for large meshes > 65k verts)
                if (primitive == PrimitiveType::Triangles) { 
                    std::vector<uint32_t> result;
                    result.reserve(indices.size());
                    for(auto idx : indices) {
                        result.push_back(static_cast<uint32_t>(idx));
                    }
                    return result;
                }

                // Case 2: Triangle Strip -> Triangle List
                // This logic mimics your original strip logic but only stores indices
                std::vector<uint32_t> result;
                if (indices.size() < 3) return result;

                result.reserve((indices.size() - 2) * 3); // Approximate reservation

                bool flip = false;

                for (size_t i = 0; i + 2 < indices.size(); ++i) {
                    uint32_t i0 = indices[i + 0];
                    uint32_t i1 = indices[i + 1];
                    uint32_t i2 = indices[i + 2];

                    // Skip degenerate triangles (where any two indices are the same)
                    if (i0 == i1 || i1 == i2 || i0 == i2) {
                        flip = !flip;
                        continue;
                    }

                    if (!flip) {
                        // Even: Natural order
                        result.push_back(i0);
                        result.push_back(i1);
                        result.push_back(i2);
                    } else {
                        // Odd: Flip winding order
                        result.push_back(i1);
                        result.push_back(i0);
                        result.push_back(i2);
                    }

                    flip = !flip;
                }

                return result;
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

    inline binary::Dtype deduceDtype(const AttributeData& data) {
    return std::visit([](auto&& arg) -> binary::Dtype {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, InlineVec<uint8_t>>)   return binary::Dtype::uByte;
        if constexpr (std::is_same_v<T, InlineVec<int8_t>>)    return binary::Dtype::sByte;
        if constexpr (std::is_same_v<T, InlineVec<uint16_t>>)  return binary::Dtype::uShort;
        if constexpr (std::is_same_v<T, InlineVec<int16_t>>)   return binary::Dtype::sShort;
        if constexpr (std::is_same_v<T, InlineVec<uint32_t>>)  return binary::Dtype::uInt;
        if constexpr (std::is_same_v<T, InlineVec<int32_t>>)   return binary::Dtype::sInt;
        if constexpr (std::is_same_v<T, InlineVec<float>>)     return binary::Dtype::Float_alias;
        if constexpr (std::is_same_v<T, InlineVec<float16>>)   return binary::Dtype::Float16_alias;
        if constexpr (std::is_same_v<T, std::monostate>)       return binary::Dtype::Float;
        assert(false);
    }, data);
    }

    inline size_t elementSize(binary::Dtype dtype){
        switch (dtype) {
        case binary::Dtype::uByte:   return sizeof(uint8_t);
        case binary::Dtype::sByte:   return sizeof(int8_t);
        case binary::Dtype::uShort:  return sizeof(uint16_t);
        case binary::Dtype::sShort:  return sizeof(int16_t);
        case binary::Dtype::uInt:    return sizeof(uint32_t);
        case binary::Dtype::sInt:    return sizeof(int32_t);
        case binary::Dtype::Float:
        case binary::Dtype::Float_alias: return sizeof(float);
        case binary::Dtype::Float16:
        case binary::Dtype::Float16_alias: return sizeof(float16);
        }
        return 0;
    }

    inline const VertexAttribute* getAttributeByType(const Vertex& v, Atype t)
    {
        switch (t) {
        case binary::Atype::Position: return &v.position;
        case binary::Atype::Normal:   return &v.normal;
        case binary::Atype::Tangent:  return &v.tangent;
        case binary::Atype::Binormal: return &v.binormal;
        case binary::Atype::UV1:      return &v.uv1;
        case binary::Atype::UV2:      return &v.uv2;
        case binary::Atype::UV3:      return &v.uv3;
        case binary::Atype::unk_8:    return &v.unk_8;
        case binary::Atype::Color:    return &v.color;
        case binary::Atype::Index:    return &v.index;
        case binary::Atype::Weight:   return &v.weight;
        }
        return nullptr;
    }

    bool validateAttributeConsistency(const std::vector<Vertex>& vertices) {
        if (vertices.empty()) return true;

        const Vertex& first = vertices[0];

        auto getCount = [](const AttributeData& data) -> uint16_t {
            return std::visit([](auto&& arg) -> uint16_t {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                    return 0;
                else
                    return static_cast<uint16_t>(arg.size());
            }, data);
        };

        auto check = [&](binary::Atype type, const VertexAttribute& firstAttr) {
            for (size_t i = 1; i < vertices.size(); ++i) {
                const VertexAttribute* a = getAttributeByType(vertices[i], type);

                bool firstEmpty = firstAttr.isEmpty();
                bool thisEmpty  = a->isEmpty();

                if (firstEmpty != thisEmpty)
                    return false;

                if (!firstEmpty) {
                    // compare variant index (same dtype)
                    if (firstAttr.data.index() != a->data.index())
                        return false;

                    // compare element counts
                    if (getCount(firstAttr.data) != getCount(a->data))
                        return false;
                }
            }
            return true;
        };

        return
            check(binary::Atype::Position, first.position) &&
            check(binary::Atype::Normal,   first.normal) &&
            check(binary::Atype::Tangent,  first.tangent) &&
            check(binary::Atype::Binormal, first.binormal) &&
            check(binary::Atype::UV1,      first.uv1) &&
            check(binary::Atype::UV2,      first.uv2) &&
            check(binary::Atype::UV3,      first.uv3) &&
            check(binary::Atype::unk_8,    first.unk_8) &&
            check(binary::Atype::Color,    first.color) &&
            check(binary::Atype::Index,    first.index) &&
            check(binary::Atype::Weight,   first.weight);
    }

    struct MeshAttributeResult {
        std::vector<binary::MeshAttribute> attributes;
        uint32_t totalSizeBytes;
    };

    MeshAttributeResult buildMeshAttributes(const std::vector<Vertex>& vertices) {
        MeshAttributeResult out;
        if (vertices.empty())
            return out;

        size_t offset = 0;
        const Vertex& v0 = vertices[0];

        constexpr Atype ordered[] = {
            binary::Atype::Position, binary::Atype::Normal, binary::Atype::Tangent, binary::Atype::Binormal,
            binary::Atype::UV1, binary::Atype::UV2, binary::Atype::UV3, binary::Atype::unk_8,
            binary::Atype::Color, binary::Atype::Index, binary::Atype::Weight
        };

        auto getCount = [](const AttributeData& data) -> uint16_t {
            return std::visit([](auto&& arg) -> uint16_t {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                    return 0;
                else
                    return static_cast<uint16_t>(arg.size());
            }, data);
        };

        auto align4 = [](size_t x) {
            return (x + 3) & ~size_t(3);
        };

        for (binary::Atype at : ordered) {
            const VertexAttribute* attr = getAttributeByType(v0, at);
            if (!attr || attr->isEmpty())
                continue;

            binary::MeshAttribute desc;
            desc.atype = at;
            desc.count = getCount(attr->data);
            desc.dtype = deduceDtype(attr->data);

            // *** New: ensure 4-byte alignment ***
            offset = align4(offset);

            desc.offset = static_cast<uint16_t>(offset);
            out.attributes.push_back(desc);

            offset += desc.count * elementSize(desc.dtype);
        }

        offset = align4(offset);
        out.totalSizeBytes = static_cast<uint32_t>(offset);

        return out;
    }

    struct AttributeJob {
        size_t offset;          // Offset in the vertex buffer
        size_t count;           // Number of elements (1-4)
        binary::Dtype dtype;    // Data type
        VertexAttribute Vertex::* member; // Pointer to the class member
    };

    void unpackVertices(
        std::vector<Vertex> &vertices,
        const std::vector<binary::MeshAttribute>& descriptors,
        const std::string& packedVertices, 
        size_t vertexStride
    ) {
        if (!vertices.empty()) vertices.clear();
        
        size_t vertexCount = packedVertices.size() / vertexStride;
        vertices.resize(vertexCount);

        // --- PRE-CALCULATION STEP ---
        // Analyze descriptors ONCE, not per vertex.
        // We create a list of "jobs" to do for each vertex.
        std::vector<AttributeJob> jobs;
        jobs.reserve(descriptors.size());

        for (const auto& desc : descriptors) {
            VertexAttribute Vertex::* targetMember = nullptr;

            switch (desc.atype) {
                case Atype::Position: targetMember = &Vertex::position; break;
                case Atype::Normal:   targetMember = &Vertex::normal; break;
                case Atype::Tangent:  targetMember = &Vertex::tangent; break;
                case Atype::Binormal: targetMember = &Vertex::binormal; break;
                case Atype::UV1:      targetMember = &Vertex::uv1; break;
                case Atype::UV2:      targetMember = &Vertex::uv2; break;
                case Atype::UV3:      targetMember = &Vertex::uv3; break;
                case Atype::unk_8:    targetMember = &Vertex::unk_8; break;
                case Atype::Color:    targetMember = &Vertex::color; break;
                case Atype::Index:    targetMember = &Vertex::index; break;
                case Atype::Weight:   targetMember = &Vertex::weight; break;
                default: continue; 
            }

            if (targetMember) {
                jobs.push_back({ desc.offset, desc.count, desc.dtype, targetMember });
            }
        }

        // --- BATCH PROCESSING STEP ---
        const uint8_t* streamStart = reinterpret_cast<const uint8_t*>(packedVertices.data());

        for (size_t i = 0; i < vertexCount; ++i) {
            Vertex& v = vertices[i];
            const uint8_t* vertexPtr = streamStart + (i * vertexStride);

            // Unroll the jobs for this specific vertex
            for (const auto& job : jobs) {
                const uint8_t* dataPtr = vertexPtr + job.offset;
                VertexAttribute& attr = v.*(job.member); // Access member via pointer

                // Direct dispatch based on type
                switch (job.dtype) {
                    case binary::Dtype::uByte:         attr.setFromPtr<uint8_t>(dataPtr, job.count); break;
                    case binary::Dtype::sByte:         attr.setFromPtr<int8_t>(dataPtr, job.count); break;
                    case binary::Dtype::uShort:        attr.setFromPtr<uint16_t>(dataPtr, job.count); break;
                    case binary::Dtype::sShort:        attr.setFromPtr<int16_t>(dataPtr, job.count); break;
                    case binary::Dtype::uInt:          attr.setFromPtr<uint32_t>(dataPtr, job.count); break;
                    case binary::Dtype::sInt:          attr.setFromPtr<int32_t>(dataPtr, job.count); break;
                    case binary::Dtype::Float:
                    case binary::Dtype::Float_alias:   attr.setFromPtr<float>(dataPtr, job.count); break;
                    case binary::Dtype::Float16:
                    case binary::Dtype::Float16_alias: attr.setFromPtr<float16>(dataPtr, job.count); break;
                    default: break; 
                }
            }
        }
    }

    std::string packVertices(
        const std::vector<binary::MeshAttribute>& descriptors,
        const std::vector<Vertex>& vertices,
        size_t vertexStride)
    {
        if (vertices.empty()) return "";

        // 1. Pre-allocate buffer (zero-init to handle padding bytes cleanly)
        std::string buffer;
        buffer.resize(vertexStride * vertices.size(), 0);
        
        // Get raw pointer to beginning of buffer for fast pointer arithmetic
        uint8_t* bufferStart = reinterpret_cast<uint8_t*>(buffer.data());

        // 2. PRE-CALCULATION (Job Hoisting)
        // Map descriptors to member pointers ONCE.
        // This avoids the switch statement running inside the vertex loop.
        struct PackJob {
            size_t offset;
            VertexAttribute Vertex::* member;
        };

        std::vector<PackJob> jobs;
        jobs.reserve(descriptors.size());

        for (const auto& desc : descriptors) {
            VertexAttribute Vertex::* targetMember = nullptr;
            switch (desc.atype) {
                case Atype::Position: targetMember = &Vertex::position; break;
                case Atype::Normal:   targetMember = &Vertex::normal; break;
                case Atype::Tangent:  targetMember = &Vertex::tangent; break;
                case Atype::Binormal: targetMember = &Vertex::binormal; break;
                case Atype::UV1:      targetMember = &Vertex::uv1; break;
                case Atype::UV2:      targetMember = &Vertex::uv2; break;
                case Atype::UV3:      targetMember = &Vertex::uv3; break;
                case Atype::unk_8:    targetMember = &Vertex::unk_8; break;
                case Atype::Color:    targetMember = &Vertex::color; break;
                case Atype::Index:    targetMember = &Vertex::index; break;
                case Atype::Weight:   targetMember = &Vertex::weight; break;
                default: continue;
            }
            if (targetMember) {
                jobs.push_back({ desc.offset, targetMember });
            }
        }

        // 3. PROCESSING LOOP
        for (size_t i = 0; i < vertices.size(); ++i) {
            const Vertex& v = vertices[i];
            uint8_t* vertexBase = bufferStart + (i * vertexStride);

            for (const auto& job : jobs) {
                // Fast access using the pre-calculated member pointer
                const VertexAttribute& attr = v.*(job.member);

                if (attr.isEmpty()) continue;

                // Visit the variant (now holding InlineVec)
                std::visit([&](auto&& vec) {
                    using T = std::decay_t<decltype(vec)>;

                    if constexpr (std::is_same_v<T, std::monostate>) {
                        return; 
                    } else {
                        // vec is InlineVec<Type>.
                        // We use *vec.data() to deduce the inner type size.
                        size_t bytesToCopy = vec.size() * sizeof(*vec.data());
                        
                        // Safety check (optional, depending on trust of data)
                        // if (job.offset + bytesToCopy <= vertexStride) 
                        memcpy(vertexBase + job.offset, vec.data(), bytesToCopy);
                    }
                }, attr.data);
            }
        }

        return buffer;
    }

    std::optional<std::reference_wrapper<binary::MeshAttribute>>
    getAttributeByAtype(std::vector<binary::MeshAttribute> &attr, binary::Atype type) {
        for (binary::MeshAttribute &a : attr) {
            if (a.atype == type) {
                return a;
            }
        }
        return std::nullopt;
    }

    bool deleteAttributeByAtype(std::vector<binary::MeshAttribute> &attr, binary::Atype type) {
        for (auto it = attr.begin(); it != attr.end(); ++it) {
            if (it->atype == type) {
                attr.erase(it);
                return true;
            }
        }
        return false;
    }

    std::string getAtype(const binary::MeshAttribute &m){
        switch (m.atype) {
            case Atype::Position: return "position";
            case Atype::Normal:   return "normal";
            case Atype::Tangent:  return "tangent";
            case Atype::Binormal: return "binormal";
            case Atype::UV1:      return "uv1";
            case Atype::UV2:      return "uv2";
            case Atype::UV3:      return "uv3";
            case Atype::unk_8:    return "unk_8";
            case Atype::Color:    return "color";
            case Atype::Index:    return "index";
            case Atype::Weight:   return "weight";
            default: return "";
        }
    }

    std::string getDtype(const binary::MeshAttribute &m){
        switch (m.dtype) {
            case Dtype::uByte:          return "uByte";
            case Dtype::sByte:          return "sByte";
            case Dtype::uShort:         return "uShort";
            case Dtype::sShort:         return "sShort";
            case Dtype::uInt:           return "uInt";
            case Dtype::sInt:           return "sInt";
            case Dtype::Float:
            case Dtype::Float_alias:    return "float";
            case Dtype::Float16:
            case Dtype::Float16_alias:  return "float16";
            default: return "";
        }
    }
}
