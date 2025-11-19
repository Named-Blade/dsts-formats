#include <string>
#include <fstream>

#include "../geom/geom.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<Mesh>)

void bind_geom(py::module_ &m) {
    py::bind_vector<std::vector<Mesh>>(m, "MeshList");

    py::class_<Geom>(m, "Geom")
        .def(py::init<>())
        .def_readwrite("skeleton", &Geom::skeleton)
        .def_readwrite("meshes", &Geom::meshes)
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
        }, py::arg("data"), py::arg("base")=0);
}