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
        .def_property_readonly("uniform_type",
            [](const dsts::geom::ShaderUniform &u) {
                return std::visit([](auto &&arg) -> std::string {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::string>)
                        return "texture";
                    else
                        return "float";
                }, u.value);
            })
        .def_property("value",
            [](dsts::geom::ShaderUniform &u) -> py::object {
                return std::visit([&](auto &arg) -> py::object {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, std::string>) {
                        return py::cast(arg);
                    } else {
                        // FLOAT VECTOR → RETURN WRITABLE NUMPY VIEW

                        auto *vec_ptr = &arg; // std::vector<float>*

                        // Capsule ensures lifetime is tied to the C++ object
                        py::capsule base(vec_ptr, [](void*) {
                            // no deletion; C++ owns the vector
                        });

                        return py::array(
                            py::buffer_info(
                                vec_ptr->data(),                 // pointer
                                sizeof(float),                   // itemsize
                                py::format_descriptor<float>::format(), // format
                                1,                               // ndim
                                { vec_ptr->size() },             // shape
                                { sizeof(float) }                // stride
                            ),
                            base // capsule managing lifetime
                        );
                    }
                }, u.value);
            },

            [](dsts::geom::ShaderUniform &u, py::object obj) {
                // SETTER
                if (py::isinstance<py::str>(obj)) {
                    u.value = obj.cast<std::string>();
                } else {
                    // Convert any sequence-like object into a vector<float>
                    std::vector<float> vals = obj.cast<std::vector<float>>();
                    u.value = std::move(vals);
                }
            })
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
        .def_property(
            "uniforms",
            make_vector_property(&Material::uniforms).first,
            make_vector_property(&Material::uniforms).second
        )
        .def_property(
            "settings",
            make_vector_property(&Material::settings).first,
            make_vector_property(&Material::settings).second
        )
        .def("__repr__", [](const Material &m){ return "<Material :" + m.name + ">";});
}