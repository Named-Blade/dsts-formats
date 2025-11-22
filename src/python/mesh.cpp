#include <string>
#include <fstream>
#include <variant>
#include <type_traits>

// Assuming mesh.cpp/h includes the InlineVec definition from the previous step
#include "../geom/mesh.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<Vertex>)
PYBIND11_MAKE_OPAQUE(std::vector<uint16_t>)

// ---------- Convert InlineVec → NumPy array ----------
struct AttributeToNumpy {
    py::object owner;

    AttributeToNumpy(py::object owner_) : owner(std::move(owner_)) {}

    py::object operator()(const std::monostate&) const {
        return py::none();
    }

    template <typename VecT>
    py::object operator()(const VecT& vec) const {
        using RawT = std::remove_cv_t<std::remove_pointer_t<decltype(vec.data())>>;

        // shape and strides
        std::vector<size_t> shape = { static_cast<size_t>(vec.size()) };
        std::vector<size_t> strides = { static_cast<size_t>(sizeof(RawT)) };

        if constexpr (std::is_same_v<RawT, float16>) {
            // For float16 we treat the underlying storage as uint16_t
            auto data_ptr = reinterpret_cast<const uint16_t*>(vec.data());
            // dtype "e" is NumPy half — pybind11 accepts dtype string
            return py::array(
                py::dtype("e"),
                shape,
                strides,
                const_cast<uint16_t*>(data_ptr), // py::array takes non-const void*
                owner // owner keeps the parent (and thus the memory) alive
            );
        } else {
            return py::array(
                py::dtype::of<RawT>(),
                shape,
                strides,
                const_cast<RawT*>(vec.data()),
                owner
            );
        }
    }
};

// ---------- Convert NumPy array → InlineVec<T> ----------
template <typename T>
void numpy_to_inline(InlineVec<T>& out_vec, const py::array& arr) {
    if (!py::dtype::of<T>().is(arr.dtype())) {
        throw std::runtime_error("Incorrect dtype for vertex attribute assignment");
    }

    auto size = arr.size();
    if (size > 4) {
        throw std::runtime_error("Vertex attributes cannot have more than 4 elements");
    }

    out_vec.resize(size);
    std::memcpy(out_vec.data(), arr.data(), size * sizeof(T));
}

// Specialization/Overload for float16
inline void numpy_to_inline_f16(InlineVec<float16>& out_vec, const py::array& arr) {
    if (arr.dtype().kind() != 'f' || arr.itemsize() != 2) {
        throw std::runtime_error("Expected float16 array (dtype='e')");
    }

    auto size = arr.size();
    if (size > 4) {
        throw std::runtime_error("Vertex attributes cannot have more than 4 elements");
    }

    out_vec.resize(size);
    std::memcpy(out_vec.data(), arr.data(), size * sizeof(uint16_t));
}

// ---------- Assign NumPy array → VertexAttribute ----------
void assign_numpy(VertexAttribute& attr, const py::object& obj) {
    if (obj.is_none()) {
        attr.data = std::monostate{};
        return;
    }

    py::array arr = py::cast<py::array>(obj);

    // Basic sanity check on dtype kinds
    switch (arr.dtype().kind()) {
        case 'u':  // unsigned int
        case 'i':  // signed int
        case 'f':  // float
            break;
        default:
            throw std::runtime_error("Unsupported dtype for vertex attribute");
    }

    // Visit the variant to write to the correct InlineVec type
    std::visit([&](auto& current) {
        using VecType = std::decay_t<decltype(current)>;

        if constexpr (std::is_same_v<VecType, std::monostate>) {
            // If the attribute is currently empty (monostate), we don't know 
            // which numeric type to create. The user must initialize the VertexAttribute 
            // with a type before assigning raw data, or use the Python constructor.
            throw std::runtime_error("Cannot assign numpy array to an empty/uninitialized VertexAttribute. Initialize it with a type first.");
        }
        else {
            // Deduce T from InlineVec<T>
            using T = std::remove_pointer_t<decltype(current.data())>;

            if constexpr (std::is_same_v<T, float16>) {
                numpy_to_inline_f16(current, arr);
            } else {
                numpy_to_inline<T>(current, arr);
            }
        }
    }, attr.data);
}

// Convenience wrapper
static py::object to_numpy(py::object &self, const VertexAttribute& a) {
    return std::visit(AttributeToNumpy(self), a.data);
}

// ---------- Property Binder Helper ----------
template <typename VertexType, typename MemberType>
void def_vertex_property(py::class_<VertexType>& cls, const char* name, MemberType VertexType::*member) {
    cls.def_property(
        name,
        // Getter
        [member](py::object &py_self) {
            auto& v = py_self.cast<VertexType&>();
            return to_numpy(py_self, v.*member);
        },
        // Setter
        [member](VertexType& v, py::object a) {
            if constexpr (std::is_same_v<MemberType, VertexAttribute>) {
                if (py::isinstance<VertexAttribute>(a)) {
                    v.*member = a.cast<VertexAttribute>();
                    return;
                }
            }
            assign_numpy(v.*member, a);
        }
    );
}

// ---------- Module Definition ----------
void bind_mesh(py::module_ &m) {
    // float16 binding
    py::class_<float16>(m, "float16")
        .def(py::init<>())
        .def(py::init<float>())
        .def("__float__", [](const float16 &h){ return static_cast<float>(h); })
        .def("__repr__", [](const float16 &h){ return "<float16 " + std::to_string(static_cast<float>(h)) + ">"; });

    // VertexAttribute binding
    py::class_<VertexAttribute>(m, "VertexAttribute")
        .def(py::init<>())
        .def(py::init<std::string, size_t>())
        .def("as_numpy", [](py::object self) {
            VertexAttribute &attr = self.cast<VertexAttribute&>();
            return to_numpy(self, attr);
        })
        .def("assign",   [](VertexAttribute& a, py::object v) { assign_numpy(a, v); })
        .def("is_empty", &VertexAttribute::isEmpty);

    // Vertex binding
    py::class_<Vertex> pyVertex(m, "Vertex");
    pyVertex.def(py::init<>());

    def_vertex_property(pyVertex, "position", &Vertex::position);
    def_vertex_property(pyVertex, "normal",   &Vertex::normal);
    def_vertex_property(pyVertex, "tangent",  &Vertex::tangent);
    def_vertex_property(pyVertex, "binormal", &Vertex::binormal);
    def_vertex_property(pyVertex, "uv1",      &Vertex::uv1);
    def_vertex_property(pyVertex, "uv2",      &Vertex::uv2);
    def_vertex_property(pyVertex, "uv3",      &Vertex::uv3);
    def_vertex_property(pyVertex, "color",    &Vertex::color);
    def_vertex_property(pyVertex, "index",    &Vertex::index);
    def_vertex_property(pyVertex, "weight",   &Vertex::weight);
    
    // Add repr for debugging
    pyVertex.def("__repr__", [](const Vertex& v) {
        return std::string("<Vertex pos=") + (v.position.isEmpty() ? "None" : "Set") + ">";
    });

    // Triangle binding
    py::class_<Triangle>(m, "Triangle")
        .def(py::init<>())
        .def_readwrite("v0", &Triangle::v0)
        .def_readwrite("v1", &Triangle::v1)
        .def_readwrite("v2", &Triangle::v2)
        .def_readwrite("i0", &Triangle::i0)
        .def_readwrite("i1", &Triangle::i1)
        .def_readwrite("i2", &Triangle::i2)
        .def("__repr__", [](const Triangle& t) {
            return "<Triangle indices=(" + 
                   std::to_string(t.i0) + ", " + 
                   std::to_string(t.i1) + ", " + 
                   std::to_string(t.i2) + ")>";
        });

    // Vector bindings
    py::bind_vector<std::vector<Vertex>>(m, "VertexList");
    py::bind_vector<std::vector<uint16_t>>(m, "IndexList");

    // MeshAttribute binding
    py::class_<MeshAttribute>(m, "MeshAttribute")
        .def_readonly("count", &MeshAttribute::count)
        .def_readonly("offset", &MeshAttribute::offset)
        .def_property_readonly("atype", [](const MeshAttribute &m){return getAtype(m);})
        .def_property_readonly("dtype", [](const MeshAttribute &m){return getDtype(m);});

    // Mesh binding
    py::class_<Mesh>(m, "Mesh")
        .def(py::init<>())
        .def_property("name", 
            [](const Mesh &m) { return m.name; }, 
            &Mesh::setName)
        .def_readonly("name_hash", &Mesh::name_hash)
        .def_readwrite("flag_0", &Mesh::flag_0)
        .def_readwrite("flag_1", &Mesh::flag_1)
        .def_readwrite("flag_2", &Mesh::flag_2)
        .def_readwrite("flag_3", &Mesh::flag_3)
        .def_readwrite("flag_4", &Mesh::flag_4)
        .def_readwrite("flag_5", &Mesh::flag_5)
        .def_readwrite("flag_6", &Mesh::flag_6)
        .def_readwrite("flag_7", &Mesh::flag_7)
        .def_property("vertices",
            make_vector_property(&Mesh::vertices).first,
            make_vector_property(&Mesh::vertices).second
        )
        .def_property("indices",
            make_vector_property(&Mesh::indices).first,
            make_vector_property(&Mesh::indices).second
        )
        .def_property("matrix_palette",
            make_vector_property(&Mesh::matrix_palette).first,
            make_vector_property(&Mesh::matrix_palette).second
        )
        .def_readwrite("material", &Mesh::material)
        .def("get_indices", &Mesh::get_indices_as_triangle_list)
        .def("pack_vertices", [](const Mesh &m) {
            auto result = buildMeshAttributes(m.vertices);
            std::string packed = packVertices(result.attributes, m.vertices, result.totalSizeBytes);
            return std::make_tuple(result.attributes, result.totalSizeBytes, py::bytes(packed));
        })
        .def("unpack_vertices", [](Mesh &m,
            const std::vector<binary::MeshAttribute> &attributes, 
            const size_t &vertexStride, 
            const py::bytes &packedVertices
        ){
            unpackVertices(m.vertices, attributes, packedVertices, vertexStride);
        })
        .def("__repr__",[](const Mesh &m){ return "<Mesh: " + m.name + ">"; });
}