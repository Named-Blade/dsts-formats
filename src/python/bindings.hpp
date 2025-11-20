#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/eval.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);

namespace py = pybind11;

template <typename Class, typename VecType>
auto make_vector_property(VecType Class::*member) {
    return std::make_pair(
        // getter
        [member](const Class &obj) -> const VecType& {
            return obj.*member;
        },
        // setter
        [member](Class &obj, py::object py_obj) {
            if (py::isinstance<VecType>(py_obj)) {
                obj.*member = py_obj.cast<VecType>();
                return;
            }

            VecType vec;
            for (auto item : py_obj) {
                vec.push_back(py::cast<typename VecType::value_type>(item));
            }
            obj.*member = std::move(vec);
        }
    );
}

void header_bindings(pybind11::module_ &m) {
    //pass
}