#include <string>
#include <fstream>

#include "../geom/skeleton.cpp"
#include "bindings.hpp"

namespace py = pybind11;
using namespace dsts::geom;
using namespace dsts::geom::binary;

void bind_skeleton(py::module_ &m) {
    // BoneTransform
    py::class_<BoneTransform>(m, "BoneTransform")
        .def(py::init<>())
        // quaternion elements
        .def_property("qx", [](BoneTransform &b){ return b.quaternion[0]; }, [](BoneTransform &b, float v){ b.quaternion[0] = v; })
        .def_property("qy", [](BoneTransform &b){ return b.quaternion[1]; }, [](BoneTransform &b, float v){ b.quaternion[1] = v; })
        .def_property("qz", [](BoneTransform &b){ return b.quaternion[2]; }, [](BoneTransform &b, float v){ b.quaternion[2] = v; })
        .def_property("qw", [](BoneTransform &b){ return b.quaternion[3]; }, [](BoneTransform &b, float v){ b.quaternion[3] = v; })
        // position elements
        .def_property("px", [](BoneTransform &b){ return b.position[0]; }, [](BoneTransform &b, float v){ b.position[0] = v; })
        .def_property("py", [](BoneTransform &b){ return b.position[1]; }, [](BoneTransform &b, float v){ b.position[1] = v; })
        .def_property("pz", [](BoneTransform &b){ return b.position[2]; }, [](BoneTransform &b, float v){ b.position[2] = v; })
        .def_property("pw", [](BoneTransform &b){ return b.position[3]; }, [](BoneTransform &b, float v){ b.position[3] = v; })
        // scale elements
        .def_property("sx", [](BoneTransform &b){ return b.scale[0]; }, [](BoneTransform &b, float v){ b.scale[0] = v; })
        .def_property("sy", [](BoneTransform &b){ return b.scale[1]; }, [](BoneTransform &b, float v){ b.scale[1] = v; })
        .def_property("sz", [](BoneTransform &b){ return b.scale[2]; }, [](BoneTransform &b, float v){ b.scale[2] = v; })
        .def_property("sw", [](BoneTransform &b){ return b.scale[3]; }, [](BoneTransform &b, float v){ b.scale[3] = v; });

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