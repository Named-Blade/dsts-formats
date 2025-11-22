#include <string>
#include <fstream>

#include "../geom/mesh.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<Vertex>)
PYBIND11_MAKE_OPAQUE(std::vector<uint16_t>)

// ---------- Convert C++ vector → NumPy array ----------
struct AttributeToNumpy {
    py::object operator()(const std::monostate&) const {
        return py::none();
    }

    template <typename T>
    py::object operator()(const std::vector<T>& vec) const {
        return py::array(
            py::dtype::of<T>(),
            vec.size(),
            vec.data(),
            py::cast(vec)   // keep alive
        );
    }

    py::object operator()(const std::vector<float16>& vec) const {
        return py::array(
            py::dtype("e"),
            vec.size(),
            reinterpret_cast<const uint16_t*>(vec.data()),
            py::cast(vec)
        );
    }
};

// ---------- Convert NumPy array → C++ vector<T> ----------
template <typename T>
std::vector<T> numpy_to_vec(const py::array& arr) {
    if (!py::dtype::of<T>().is(arr.dtype()))
        throw std::runtime_error("Incorrect dtype for assignment");

    std::vector<T> vec(arr.size());
    std::memcpy(vec.data(), arr.data(), arr.size() * sizeof(T));
    return vec;
}

inline std::vector<float16> numpy_to_vec_f16(const py::array& arr) {
    if (arr.dtype().kind() != 'f' || arr.itemsize() != 2)
        throw std::runtime_error("Expected float16 array");

    std::vector<float16> vec(arr.size());
    std::memcpy(vec.data(), arr.data(), arr.size() * sizeof(uint16_t));
    return vec;
}

// ---------- Assign NumPy array → VertexAttribute ----------
void assign_numpy(VertexAttribute& attr, const py::object& obj) {
    if (obj.is_none()) {
        attr.data = std::monostate{};
        return;
    }

    py::array arr = py::cast<py::array>(obj);

    switch (arr.dtype().kind()) {
        case 'u':  // unsigned
        case 'i':  // signed
        case 'f':  // float
            break;
        default:
            throw std::runtime_error("Unsupported dtype for vertex attribute");
    }

    // Match current variant type so Python writes to correct vector type
    std::visit([&](auto& current) {
        using T = std::decay_t<decltype(current)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            throw std::runtime_error("Cannot assign to empty attribute without knowing type.");
        }
        else if constexpr (std::is_same_v<T, std::vector<float16>>) {
            current = numpy_to_vec_f16(arr);
        }
        else {
            current = numpy_to_vec<typename T::value_type>(arr);
        }
    }, attr.data);
}

// Convenience
static py::object to_numpy(const VertexAttribute& a) {
    return std::visit(AttributeToNumpy{}, a.data);
}

template <typename VertexType, typename MemberType>
void def_vertex_property(py::class_<VertexType>& cls, const char* name, MemberType VertexType::*member) {
    cls.def_property(
        name,
        [member](const VertexType& v) {
            return to_numpy(v.*member);  // convert member to numpy array
        },
        [member](VertexType& v, py::object a) {
            // Special case: VertexAttribute
            if constexpr (std::is_same_v<MemberType, VertexAttribute>) {
                if (py::isinstance<VertexAttribute>(a)) {
                    v.*member = a.cast<VertexAttribute>();
                } else {
                    throw std::runtime_error("Expected VertexAttribute");
                }
            } else {
                assign_numpy(v.*member, a);  // assign from numpy array
            }
        }
    );
}

void bind_mesh(py::module_ &m) {
    py::class_<float16>(m, "float16")
        .def(py::init<>())
        .def(py::init<float>())
        .def("__float__", [](const float16 &h){ return static_cast<float>(h); });

    py::class_<VertexAttribute>(m, "VertexAttribute")
        .def(py::init<>())
        .def(py::init<std::string, size_t>())
        .def("as_numpy", [](const VertexAttribute& a) { return to_numpy(a); })
        .def("assign",   [](VertexAttribute& a, py::object v) { assign_numpy(a, v); });

    py::class_<Vertex> pyVertex(m, "Vertex");
    pyVertex.def(py::init<>());

    def_vertex_property(pyVertex, "position", &Vertex::position);
    def_vertex_property(pyVertex, "normal", &Vertex::normal);
    def_vertex_property(pyVertex, "tangent", &Vertex::tangent);
    def_vertex_property(pyVertex, "binormal", &Vertex::binormal);
    def_vertex_property(pyVertex, "uv1", &Vertex::uv1);
    def_vertex_property(pyVertex, "uv2", &Vertex::uv2);
    def_vertex_property(pyVertex, "uv3", &Vertex::uv3);
    def_vertex_property(pyVertex, "color", &Vertex::color);
    def_vertex_property(pyVertex, "index", &Vertex::index);
    def_vertex_property(pyVertex, "weight", &Vertex::weight);

    py::class_<Triangle>(m, "Triangle")
        .def(py::init<>())
        .def_readwrite("v0", &Triangle::v0)
        .def_readwrite("v1", &Triangle::v1)
        .def_readwrite("v2", &Triangle::v2)
        .def_readwrite("i0", &Triangle::i0)
        .def_readwrite("i1", &Triangle::i1)
        .def_readwrite("i2", &Triangle::i2)
        .def("__repr__", [](const Triangle& t) {
            return "<Triangle v0=" + std::string(py::repr(py::cast(t.v0))) +
                   " v1=" + std::string(py::repr(py::cast(t.v1))) +
                   " v2=" + std::string(py::repr(py::cast(t.v2))) + ">";
        });

    py::bind_vector<std::vector<Vertex>>(m, "VertexList");
    py::bind_vector<std::vector<uint16_t>>(m, "IndexList");

    py::class_<MeshAttribute>(m, "MeshAttribute")
        .def_readonly("count", &MeshAttribute::count)
        .def_readonly("offset", &MeshAttribute::offset)
        .def_property_readonly("atype", [](const MeshAttribute &m){return getAtype(m);})
        .def_property_readonly("dtype", [](const MeshAttribute &m){return getDtype(m);});

    py::class_<Mesh>(m, "Mesh")
        .def(py::init<>())
        .def_property(
            "name",
            [](const Mesh &m) { return m.name; },
            &Mesh::setName
        )
        .def_readonly("name_hash", &Mesh::name_hash)
        .def_readwrite("flag_0", &Mesh::flag_0)
        .def_readwrite("flag_1", &Mesh::flag_1)
        .def_readwrite("flag_2", &Mesh::flag_2)
        .def_readwrite("flag_3", &Mesh::flag_3)
        .def_readwrite("flag_4", &Mesh::flag_4)
        .def_readwrite("flag_5", &Mesh::flag_5)
        .def_readwrite("flag_6", &Mesh::flag_6)
        .def_readwrite("flag_7", &Mesh::flag_7)
        .def_property(
            "vertices",
            make_vector_property(&Mesh::vertices).first,
            make_vector_property(&Mesh::vertices).second
        )
        .def_property(
            "indices",
            make_vector_property(&Mesh::indices).first,
            make_vector_property(&Mesh::indices).second
        )
        .def_property(
            "matrix_palette",
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
        .def("__repr__",[](const Mesh &m){ return "<Mesh :"+ m.name +">";});
}