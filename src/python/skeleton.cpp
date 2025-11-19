#include <string>
#include <fstream>

#include "../utils/memory_buffer.cpp"
#include "../geom/skeleton.cpp"
#include "bindings.hpp"

using namespace dsts::geom;
using namespace dsts::geom::binary;

PYBIND11_MAKE_OPAQUE(std::vector<std::shared_ptr<Bone>>)

void bind_skeleton(py::module_ &m) {
    // BoneTransform
    py::class_<BoneTransform> boneTransform(m, "BoneTransform");
    boneTransform.def(py::init<>());

    // Helper lambda to bind properties
    auto bind_array = [&](auto BoneTransform::*member, const char* name) {
        boneTransform.def_property(
            name,
            [member,name](BoneTransform &bt) {
                return py::array_t<float>(
                    {4},                          // shape
                    {sizeof(float)},              // stride
                    bt.*member,                   // pointer to data (no copy)
                    py::cast(&bt)                 // base object (keeps alive)
                );
            },
            [member,name](BoneTransform &bt, py::array_t<float> arr) {
                if (arr.size() != 4)
                    throw std::runtime_error(std::string(name) + " must have size 4");
                std::memcpy(bt.*member, arr.data(), 4*sizeof(float));
            }
        );
    };

    bind_array(&BoneTransform::quaternion, "quaternion");
    bind_array(&BoneTransform::position, "position");
    bind_array(&BoneTransform::scale, "scale");

    boneTransform.def("__repr__", [](const BoneTransform &bt) {
        auto array_repr = [&](const float* ptr) {
            py::array_t<float> arr({4}, {sizeof(float)}, const_cast<float*>(ptr));
            return py::repr(arr).cast<std::string>();
        };

        return "<BoneTransform quaternion=" + array_repr(bt.quaternion) +
            ", position=" + array_repr(bt.position) +
            ", scale=" + array_repr(bt.scale) + ">";
    });

    // Bone
    py::class_<Bone, std::shared_ptr<Bone>>(m, "Bone")
        .def(py::init<>())
        .def_property(
            "name",
            [](const Bone &b) { return b.name; },
            &Bone::setName
        )
        .def_readonly("name_hash", &Bone::name_hash) 
        .def_readwrite("transform", &Bone::transform)
        .def_readwrite("transform_actual", &Bone::transform_actual)
        .def_readwrite("is_effect", &Bone::is_effect)
        .def_readwrite("parent", &Bone::parent)
        .def("__repr__", [](const Bone &b){ return "<Bone :" + b.name + ">";});

    // FloatChannel
    // py::class_<FloatChannel>(m, "FloatChannel")
    //     .def(py::init<>())
    //     .def_readwrite("name_hash", &FloatChannel::name_hash)
    //     .def_readwrite("array_index", &FloatChannel::array_index)
    //     .def_readwrite("flags", &FloatChannel::flags);

    // Skeleton
    py::bind_vector<std::vector<std::shared_ptr<Bone>>>(m, "BoneList");

    py::class_<Skeleton>(m, "Skeleton")
        .def(py::init<>())
        .def_property(
            "bones",
            [](const Skeleton &s) {
                return &s.bones;
            },
            [](Skeleton &s, py::object obj) {
                if (py::isinstance<std::vector<std::shared_ptr<Bone>>>(obj)) {
                    s.bones = obj.cast<std::vector<std::shared_ptr<Bone>>>();
                    return;
                }
                std::vector<std::shared_ptr<Bone>> bones;
                for (auto item : obj) {
                    bones.push_back(py::cast<std::shared_ptr<Bone>>(item));
                }
                s.bones = std::move(bones);
            }
        )
        //.def_readwrite("float_channels", &Skeleton::float_channels)
        .def_static("from_bytes", [](py::bytes data, int skeleton_base, int base){
            Skeleton s;
            std::string buf = data;
            std::istringstream f(buf, std::ios::binary);
            s.read(f, skeleton_base, base);
            return s;
        }, py::arg("data"), py::arg("skeleton_base")=0, py::arg("base")=0)
        .def_static("from_file", [](py::str filename, int skeleton_base, int base){
            Skeleton s;
            std::ifstream f(filename, std::ios::binary);
            if (!f.good()) {
                PyErr_SetString(PyExc_FileNotFoundError, ("File not found: " + std::string(filename)).c_str());
                throw py::error_already_set();
            }
            s.read(f, skeleton_base, base);
            f.close();
            return s;
        }, py::arg("data"), py::arg("skeleton_base")=0, py::arg("base")=0)
        .def("to_bytes", [](Skeleton &s) {
            MemoryStream f;
            s.write(f);
            return py::bytes(f.str());
        })
        .def("to_file", [](Skeleton &s, py::str filename){
            std::ofstream f(filename, std::ios::binary);
            s.write(f);
            f.close();
        }, py::arg("data"));
}