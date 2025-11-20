#include <string>
#include <fstream>

#include "../geom/material.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<dsts::geom::ShaderUniform>)
PYBIND11_MAKE_OPAQUE(std::vector<dsts::geom::ShaderSetting>)

void bind_material(py::module_ &m) {
    //Shader
    py::class_<dsts::geom::Shader>(m, "Shader")
        .def_property(
            "name",
            &dsts::geom::Shader::getShaderName,
            &dsts::geom::Shader::setShaderName
        )
        .def("__repr__",[](const dsts::geom::Shader &s){ return "<Shader :" + s.getShaderName() + ">";});
    
    //Uniform
    py::class_<dsts::geom::ShaderUniform>(m, "ShaderUniform")
        .def(py::init<>())
        .def_property("parameter_id",
            &dsts::geom::ShaderUniform::getParameterId,
            &dsts::geom::ShaderUniform::setParameterId)
        .def_property("parameter_name",
            &dsts::geom::ShaderUniform::getParameterName,
            &dsts::geom::ShaderUniform::setParameterName)
        .def("__repr__", [](const dsts::geom::ShaderUniform &u){return "<Uniform :" + u.parameter_name + ">";});

    //Setting
    py::class_<dsts::geom::ShaderSetting>(m, "ShaderSetting")
        .def(py::init<>())
        .def_property("parameter_id",
            &dsts::geom::ShaderSetting::getParameterId,
            &dsts::geom::ShaderSetting::setParameterId)
        .def_property("parameter_name",
            &dsts::geom::ShaderSetting::getParameterName,
            &dsts::geom::ShaderSetting::setParameterName)
        .def("__repr__", [](const dsts::geom::ShaderSetting &s){return "<Setting :" + s.parameter_name + ">";});

    py::bind_vector<std::vector<dsts::geom::ShaderUniform>>(m, "ShaderUniformList");
    py::bind_vector<std::vector<dsts::geom::ShaderSetting>>(m, "ShaderSettingList");

    // Material
    py::class_<Material, std::shared_ptr<Material>>(m, "Material")
        .def(py::init<>())
        .def_property(
            "name",
            [](const Material &m) { return m.name; },
            &Material::setName
        )
        .def_readonly("name_hash", &Material::name_hash)
        .def_readonly("shaders", &Material::shaders)
        .def_readwrite("uniforms", &Material::uniforms)
        .def_readwrite("settings", &Material::settings)
        .def("__repr__", [](const Material &m){ return "<Material :" + m.name + ">";});
}