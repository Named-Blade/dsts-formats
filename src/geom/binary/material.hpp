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

    struct TexturePayload{
        uint64_t texture_name_offset;
        uint32_t texture_name_length;
        uint32_t unknown_0xC;
    };

    struct FloatPayload{
        float floats[4];
    };

    struct Payload {
        uint8_t payload[0x10];
    };

    struct ShaderUniform{
        union {
            Payload payload;
            FloatPayload floats;
            TexturePayload texture;
        } payload;
        uint16_t parameter_id;
        uint16_t float_count;
        uint32_t unknown_0x14;
        uint32_t unknown_0x18;
        uint32_t unknown_0x1C;
    };

    struct ShaderSetting{
        Payload payload;
        uint16_t parameter_id;
        uint16_t unknown_0x12;
        uint32_t unknown_0x14;
        uint32_t unknown_0x18;
        uint32_t unknown_0x1C;
    };

    struct ShaderParamDef {
        uint32_t id;
        uint8_t unk[0x14];
        char name[0x20];
    };

    constexpr uint64_t shaderParamNum = 791;

    #pragma pack(pop)
}
