#pragma once

#include <iostream>
#include <cstddef> 
#include <vector>
#include <array>
#include <string>
#include <memory>

#include "../utils/hash.cpp"
#include "binary/mesh.hpp"

namespace dsts::geom
{  
    class Mesh{
        public:
            uint32_t name_hash;
            std::string name;

            void setName(std::string newName){
                name = newName;
                name_hash = crc32((const uint8_t*)name.data(),name.length());
            }
    };
}
