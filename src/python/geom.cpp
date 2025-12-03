#include <string>
#include <fstream>

#include "../geom/geom.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<Mesh>)
PYBIND11_MAKE_OPAQUE(std::vector<std::shared_ptr<Material>>)

void bind_geom(py::module_ &m) {
    py::bind_vector<std::vector<Mesh>>(m, "MeshList");
    py::bind_vector<std::vector<std::shared_ptr<Material>>>(m, "MaterialList");

    py::class_<Geom>(m, "Geom")
        .def(py::init<>())
        .def_readwrite("unknown_0x10", &Geom::unknown_0x10)
        .def_readwrite("unknown_0x30", &Geom::unknown_0x30)
        .def_readwrite("unknown_0x34", &Geom::unknown_0x34)
        .def_property("clut", [](const Geom &g){
            constexpr size_t n = sizeof(g.clut);
            return py::memoryview::from_buffer(
                reinterpret_cast<uint8_t const*>(&g.clut),
                { n },
                { sizeof(uint8_t) }
            );
        },[](Geom &g, py::bytes b){
            constexpr size_t n = sizeof(g.clut);

            py::buffer_info info(py::buffer(b).request());
            size_t input_size = info.size; 
            const uint8_t* src = static_cast<const uint8_t*>(info.ptr);
            uint8_t* value = reinterpret_cast<uint8_t*>(&g.clut);

            if (input_size >= n) {
                memcpy(value, src, n);
            } else {
                memcpy(value, src, input_size);
                memset(value + input_size, 0, n - input_size);
            }

        })
        .def_readwrite("skeleton", &Geom::skeleton)
        .def_property(
            "meshes",
            make_vector_property(&Geom::meshes).first,
            make_vector_property(&Geom::meshes).second
        )
        .def_property(
            "materials",
            make_vector_property(&Geom::materials).first,
            make_vector_property(&Geom::materials).second
        )
        .def_static("from_bytes", [](py::bytes data, int base){
            Geom geom;
            std::string buf = data;
            std::istringstream f(buf, std::ios::binary);
            geom.read(f, base);
            return geom;
        }, py::arg("data"), py::arg("base")=0)
        .def_static("from_file", [](py::str filename, int base){
            Geom geom;
            std::ifstream f(filename, std::ios::binary);
            if (!f.good()) {
                PyErr_SetString(PyExc_FileNotFoundError, ("File not found: " + std::string(filename)).c_str());
                throw py::error_already_set();
            }
            geom.read(f, base);
            f.close();
            return geom;
        }, py::arg("data"), py::arg("base")=0)
        .def("to_bytes", [](Geom &g) {
            MemoryStream f;
            g.write(f);
            return py::bytes(f.str());
        })
        .def("to_file", [](Geom &g, py::str filename){
            std::ofstream f(filename, std::ios::binary);
            g.write(f);
            f.close();
        })
        .def("copy", &Geom::copy);
}