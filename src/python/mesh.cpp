#include <string>
#include <fstream>

#include "../geom/mesh.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

void bind_mesh(py::module_ &m) {
    py::class_<Mesh>(m, "Mesh")
        .def(py::init<>())
        .def_property(
            "name",
            [](const Mesh &m) { return m.name; },
            &Mesh::setName
        )
        .def_readonly("name_hash", &Mesh::name_hash) ;
}