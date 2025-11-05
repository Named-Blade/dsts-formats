#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);

namespace py = pybind11;

template <size_t N>
struct ArrayWrapper {
    float* data;

    ArrayWrapper(float* d) : data(d) {}

    // Single element access
    float get(size_t i) const {
        if (i >= N) throw std::out_of_range("Index out of range");
        return data[i];
    }

    void set(size_t i, float v) {
        if (i >= N) throw std::out_of_range("Index out of range");
        data[i] = v;
    }

    // Slice access
    py::list get(py::slice slice) const {
        size_t start, stop, step, slicelength;
        if (!slice.compute(N, &start, &stop, &step, &slicelength))
            throw py::error_already_set();
        py::list result;
        for (size_t i = 0; i < slicelength; ++i)
            result.append(data[start + i*step]);
        return result;
    }

    void set(py::slice slice, py::iterable values) {
        size_t start, stop, step, slicelength;
        if (!slice.compute(N, &start, &stop, &step, &slicelength))
            throw py::error_already_set();
        size_t i = 0;
        for (auto v : values) {
            if (i >= slicelength) break;
            data[start + i*step] = py::cast<float>(v);
            i++;
        }
        if (i != slicelength)
            throw std::runtime_error("Slice assignment size mismatch");
    }

    std::array<float, N> get_all() const {
        std::array<float, N> arr;
        for (size_t i = 0; i < N; ++i) arr[i] = data[i];
        return arr;
    }

    void set_all(const std::array<float, N>& arr) {
        for (size_t i = 0; i < N; ++i) data[i] = arr[i];
    }

    std::string repr() const {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < N; ++i) {
            oss << data[i];
            if (i != N-1) oss << ", ";
        }
        oss << "]";
        return oss.str();
    }
};

void header_bindings(pybind11::module_ &m) {
    // Register a single wrapper class for size 4 arrays
    py::class_<ArrayWrapper<4>>(m, "ArrayWrapper4")
        .def("__getitem__", py::overload_cast<size_t>(&ArrayWrapper<4>::get, py::const_))
        .def("__getitem__", py::overload_cast<py::slice>(&ArrayWrapper<4>::get, py::const_))
        .def("__setitem__", py::overload_cast<size_t, float>(&ArrayWrapper<4>::set))
        .def("__setitem__", py::overload_cast<py::slice, py::iterable>(&ArrayWrapper<4>::set))
        .def_property("all", &ArrayWrapper<4>::get_all, &ArrayWrapper<4>::set_all)
        .def("__repr__", &ArrayWrapper<4>::repr);
}