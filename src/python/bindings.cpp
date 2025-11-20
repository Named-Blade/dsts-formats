#include "bindings.hpp"
#include "skeleton.cpp"
#include "mesh.cpp"
#include "material.cpp"
#include "geom.cpp"

PYBIND11_MODULE(dsts_formats, m) {
    header_bindings(m);
    bind_skeleton(m);
    bind_mesh(m);
    bind_material(m);
    bind_geom(m);
}
