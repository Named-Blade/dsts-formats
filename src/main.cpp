#include <cstddef>
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

#include "geom/binary/clut.hpp"
#include "geom/binary/geom.hpp"
#include "geom/binary/material.hpp"
#include "geom/binary/mesh.hpp"
#include "geom/binary/name_table.hpp"
#include "geom/binary/skeleton.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;;
        return 1;
    }

    std::string filename = argv[1];

    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << "\n";
        return 1;
    }

    dsts::geom::binary::GeomHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(dsts::geom::binary::GeomHeader));

    assert(header.version == 316);

    file.seekg(header.mesh_offset);
    dsts::geom::binary::MeshHeader *meshes = new dsts::geom::binary::MeshHeader[header.mesh_count];
    file.read(reinterpret_cast<char*>(meshes), sizeof(dsts::geom::binary::MeshHeader) * header.mesh_count);

    std::cout << meshes[3].attribute_count << std::endl;
}