#pragma endian little

import std.core;
import std.io;
import std.ptr;

struct ShaderParamDef {
    u32 id;
    u8 unk[0x14];
    char name[] @  std::ptr::relative_to_parent(0) + 0x18;
} [[fixed_size(0x38)]] ;

ShaderParamDef shaders[791] @ 0x0 ;//starts at 0x1B5FCB8 in current game version (manifest 6255606497573339907).