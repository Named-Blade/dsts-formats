#pragma once

#include "enums.hpp"

namespace dsts::geom::binary
{   
    #pragma pack(push, 1)

    struct ColorData {
        uint8_t data[4];
    };

    struct Clut {
        uint8_t unk1[0x10];
        ColorData data;
        uint8_t unk2[0xc];
    };

    #pragma pack(pop)
}
