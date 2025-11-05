#include <string>
#include <fstream>

#include "../geom/geom.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

void bind_geom(py::module_ &m) {
    py::class_<Geom>(m, "Geom")
        .def(py::init<>())
        .def_readwrite("skeleton", &Geom::skeleton)
        .def("read_from_bytes", [](Geom &s, py::bytes data, int base){
            std::string buf = data; // copy Python bytes to std::string
            std::istringstream f(buf, std::ios::binary);
            s.read(f, base);
        }, py::arg("data"), py::arg("base")=0)
        .def("read_from_file", [](Geom &s, py::str filename, int base){
            std::ifstream f(filename, std::ios::binary);
            if (!f.good()) {
                PyErr_SetString(PyExc_FileNotFoundError, ("File not found: " + std::string(filename)).c_str());
                throw py::error_already_set();
            }
            s.read(f, base);
        }, py::arg("data"), py::arg("base")=0);
}