#pragma once

#include "enums.hpp"

namespace dsts::geom::binary
{   
    #pragma pack(push, 1)

    struct Shader {
        uint32_t name_data[14];
    };

    struct MaterialHeader {
        uint32_t name_hash;
        
        Shader shaders[14];
        
        uint32_t unknown_0x314;
        uint32_t unknown_0x318;
        uint32_t unknown_0x31c;
        
        uint8_t uniform_count;
        uint8_t setting_count;

        char padding[0x2];
        
        uint16_t unknown_0x324;
        uint16_t unknown_0x326;
    };

    #pragma pack(pop)
}
