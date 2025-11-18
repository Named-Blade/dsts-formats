#include <string>
#include <fstream>

#include "../geom/geom.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<Mesh>)

void bind_geom(py::module_ &m) {
    py::bind_vector<std::vector<Mesh>>(m, "MeshList")
        .def("__repr", [](const std::vector<Mesh> &v){
            std::string out = "MeshList[";
            for (int i = 0; i < v.size(); i++) {
                out += "<Mesh :" + v[i].name + ">";
                if (i != v.size()-1) {
                    out += ", ";
                }
            }
            out += "]";
            return out;
        });
        //__repr__ doesn't overwrite for some reason in a bound vector, therefore you can have this nonsense instead.
        py::dict locals;
        py::exec(R"(
            module_name = "dsts_formats"
            module_already_loaded = module_name in __import__("sys").modules

            if module_already_loaded:
                mod = __import__(module_name)
            else:
                mod = __import__(module_name)

            mod.MeshList.__repr__ = mod.MeshList.__repr

            if not module_already_loaded:
                del __import__("sys").modules[module_name]
        )", py::globals(), locals);

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