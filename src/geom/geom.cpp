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
            Skeleton skeleton;

            void read(std::istream& f, int base = 0){
                binary::GeomHeader header;

                f.seekg(base);
                f.read(reinterpret_cast<char*>(&header), sizeof(binary::GeomHeader));

                skeleton.read(f, header.skeleton_offset, base);

                f.seekg(base+header.ibpm_offset);
                std::vector<binary::Ibpm> ibpms(header.ibpm_count);
                f.read(reinterpret_cast<char*>(ibpms.data()), sizeof(binary::Ibpm) * header.ibpm_count);

                assert(ibpms.size() == skeleton.bones.size());
                for (int i = 0; i < skeleton.bones.size(); i++) {
                    binary::Ibpm test = ibpmFromMatrix(computeInverseBindPose(skeleton.bones[i].get()));
                    assert(ibpmEqual(ibpms[i], test));
                }
                
            }
    };
}
