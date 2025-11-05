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

    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << "\n";
        return 1;
    }

    dsts::geom::Geom geom;
    geom.read(file);

    file.close();
}