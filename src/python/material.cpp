#include <string>
#include <fstream>

#include "../geom/material.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

void bind_material(py::module_ &m) {
    //Shader
    py::class_<dsts::geom::Shader>(m, "Shader")
        .def(py::init<>())
        .def_property(
            "name",
            &dsts::geom::Shader::getShaderName,
            &dsts::geom::Shader::setShaderName
        );


    // Material
    py::class_<Material, std::shared_ptr<Material>>(m, "Material")
        .def(py::init<>())
        .def_property(
            "name",
            [](const Material &m) { return m.name; },
            &Material::setName
        )
        .def_readonly("name_hash", &Material::name_hash)
        .def_readwrite("shaders", &Material::shaders)
        .def("__repr__", [](const Material &m){ return "<Material :" + m.name + ">";});
}