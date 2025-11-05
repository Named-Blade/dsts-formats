#include <string>
#include <fstream>

#include "../geom/skeleton.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

void bind_skeleton(py::module_ &m) {
    // BoneTransform
    py::class_<BoneTransform> boneTransform(m, "BoneTransform");
    boneTransform.def(py::init<>());

    // Helper lambda to bind properties
    auto bind_array = [&](auto BoneTransform::*member, const char* name) {
        boneTransform.def_property(
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