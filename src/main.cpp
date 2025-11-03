#include <cstddef>
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

#include "geom/geom.cpp"

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

    dsts::geom::Geom geom;
    geom.read(file);

    std::cout << geom.skeleton.bone_transforms[5].position[0] << std::endl;
    std::cout << geom.skeleton.bone_transforms[5].position[1] << std::endl;
    std::cout << geom.skeleton.bone_transforms[5].position[2] << std::endl;
    std::cout << geom.skeleton.bone_transforms[5].position[3] << std::endl;

    file.close();
}