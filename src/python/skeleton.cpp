#include <string>
#include <fstream>

#include "../geom/skeleton.cpp"
#include "bindings.hpp"

namespace py = pybind11;
using namespace dsts::geom;
using namespace dsts::geom::binary;

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

void bind_skeleton(py::module_ &m) {
    // BoneTransform
    py::class_<BoneTransform> bone(m, "BoneTransform");
    bone.def(py::init<>());

    // Register a single wrapper class for size 4 arrays
    py::class_<ArrayWrapper<4>>(m, "ArrayWrapper4")
        .def("__getitem__", py::overload_cast<size_t>(&ArrayWrapper<4>::get, py::const_))
        .def("__getitem__", py::overload_cast<py::slice>(&ArrayWrapper<4>::get, py::const_))
        .def("__setitem__", py::overload_cast<size_t, float>(&ArrayWrapper<4>::set))
        .def("__setitem__", py::overload_cast<py::slice, py::iterable>(&ArrayWrapper<4>::set))
        .def_property("all", &ArrayWrapper<4>::get_all, &ArrayWrapper<4>::set_all)
        .def("__repr__", &ArrayWrapper<4>::repr);

    // Helper lambda to bind properties
    auto bind_array = [&](auto BoneTransform::*member, const char* name) {
        bone.def_property(
            name,
            [member](BoneTransform &b) { return ArrayWrapper<4>(b.*member); },
            [member](BoneTransform &b, const std::array<float, 4>& arr) {
                ArrayWrapper<4>(b.*member).set_all(arr);
            }
        );
    };

    bind_array(&BoneTransform::quaternion, "quaternion");
    bind_array(&BoneTransform::position, "position");
    bind_array(&BoneTransform::scale, "scale");

    // Bone
    py::class_<Bone, std::shared_ptr<Bone>>(m, "Bone")
        .def(py::init<>())
        .def_readwrite("name_hash", &Bone::name_hash)
        .def_readwrite("name", &Bone::name)
        .def_readwrite("transform", &Bone::transform)
        .def_readwrite("parent", &Bone::parent);

    // FloatChannel
    py::class_<FloatChannel>(m, "FloatChannel")
        .def(py::init<>())
        .def_readwrite("name_hash", &FloatChannel::name_hash)
        .def_readwrite("array_index", &FloatChannel::array_index)
        .def_readwrite("flags", &FloatChannel::flags);

    // Skeleton
    py::class_<Skeleton>(m, "Skeleton")
        .def(py::init<>())
        .def_readwrite("bones", &Skeleton::bones)
        .def("add_bone", [](Skeleton &s, std::shared_ptr<Bone> b){
            s.bones.push_back(b);
        })
        .def("delete_bone", [](Skeleton &s, size_t index){
            if (index >= s.bones.size()) throw std::out_of_range("Bone index out of range");
            s.bones.erase(s.bones.begin() + index);
        })
        .def_readwrite("float_channels", &Skeleton::float_channels)
        // Note: read method requires a Python wrapper to pass bytes
        .def("read_from_bytes", [](Skeleton &s, py::bytes data, int skeleton_base, int base){
            std::string buf = data; // copy Python bytes to std::string
            std::istringstream f(buf, std::ios::binary);
            s.read(f, skeleton_base, base);
        }, py::arg("data"), py::arg("skeleton_base")=0, py::arg("base")=0)
        .def("read_from_file", [](Skeleton &s, py::str filename, int skeleton_base, int base){
            std::ifstream f(filename, std::ios::binary);
            s.read(f, skeleton_base, base);
        }, py::arg("data"), py::arg("skeleton_base")=0, py::arg("base")=0);
}