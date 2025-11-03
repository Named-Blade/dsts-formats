#pragma once

#include "binary/clut.hpp"
#include "binary/enums.hpp"
#include "binary/geom.hpp"
#include "binary/material.hpp"
#include "binary/mesh.hpp"
#include "binary/name_table.hpp"
#include "binary/skeleton.hpp"

#include "skeleton.cpp"

namespace dsts::geom
{  
    class Geom {
        public:
        binary::GeomHeader header;
        

        Skeleton skeleton;

        void read(std::istream& f, int base = 0){
            f.seekg(base);
            f.read(reinterpret_cast<char*>(&header), sizeof(binary::GeomHeader));

            skeleton.read(f, header.skeleton_offset, base);
        }
    };
}
