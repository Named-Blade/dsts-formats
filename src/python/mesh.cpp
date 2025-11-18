#include <string>
#include <fstream>

#include "../geom/mesh.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<Vertex>)

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

void bind_mesh(py::module_ &m) {
    py::class_<float16>(m, "float16")
        .def(py::init<>())
        .def(py::init<float>())
        .def("__float__", [](const float16 &h){ return static_cast<float>(h); });

    py::class_<VertexAttribute>(m, "VertexAttribute")
        .def("as_numpy", [](const VertexAttribute& a) { return to_numpy(a); })
        .def("assign",   [](VertexAttribute& a, py::object v) { assign_numpy(a, v); });

    py::class_<Vertex>(m, "Vertex")
        .def_property(
            "position",
            [](const Vertex& v) { return to_numpy(v.position); },
            [](Vertex& v, py::object a) { assign_numpy(v.position, a); }
        )
        .def_property(
            "normal",
            [](const Vertex& v) { return to_numpy(v.normal); },
            [](Vertex& v, py::object a) { assign_numpy(v.normal, a); }
        )
        .def_property(
            "tangent",
            [](const Vertex& v) { return to_numpy(v.tangent); },
            [](Vertex& v, py::object a) { assign_numpy(v.tangent, a); }
        )
        .def_property(
            "binormal",
            [](const Vertex& v) { return to_numpy(v.binormal); },
            [](Vertex& v, py::object a) { assign_numpy(v.binormal, a); }
        )
        .def_property(
            "uv1",
            [](const Vertex& v) { return to_numpy(v.uv1); },
            [](Vertex& v, py::object a) { assign_numpy(v.uv1, a); }
        )
        .def_property(
            "uv2",
            [](const Vertex& v) { return to_numpy(v.uv2); },
            [](Vertex& v, py::object a) { assign_numpy(v.uv2, a); }
        )
        .def_property(
            "uv3",
            [](const Vertex& v) { return to_numpy(v.uv3); },
            [](Vertex& v, py::object a) { assign_numpy(v.uv3, a); }
        )
        .def_property(
            "color",
            [](const Vertex& v) { return to_numpy(v.color); },
            [](Vertex& v, py::object a) { assign_numpy(v.color, a); }
        )
        .def_property(
            "index",
            [](const Vertex& v) { return to_numpy(v.index); },
            [](Vertex& v, py::object a) { assign_numpy(v.index, a); }
        )
        .def_property(
            "weight",
            [](const Vertex& v) { return to_numpy(v.weight); },
            [](Vertex& v, py::object a) { assign_numpy(v.weight, a); }
        );

    py::bind_vector<std::vector<Vertex>>(m, "VertexList");

    py::class_<Mesh>(m, "Mesh")
        .def(py::init<>())
        .def_property(
            "name",
            [](const Mesh &m) { return m.name; },
            &Mesh::setName
        )
        .def_readonly("name_hash", &Mesh::name_hash)
        .def_readwrite("vertices", &Mesh::vertices)
        .def("__repr__",[](const Mesh &m){ return "<Mesh :"+ m.name +">";});
}