#pragma once

#include <cstdint>
#include <cstddef>

uint32_t reflect(uint32_t data, size_t bits) {
    uint32_t reflection = 0;
    for (size_t i = 0; i < bits; ++i) {
        if (data & 0x01) {
            reflection |= (1 << ((bits - 1) - i));
        }
        data >>= 1;
    }
    return reflection;
}

uint32_t crc32(const uint8_t* data, size_t length, 
               uint32_t polynomial = 0x04C11DB7,
               uint32_t initial = 0xFFFFFFFF,
               uint32_t finalXor = 0x0,
               bool reflectIn = true,
               bool reflectOut = true) {
    uint32_t crc = initial;

    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        if (reflectIn) {
            byte = reflect(byte, 8);
        }
        crc ^= static_cast<uint32_t>(byte) << 24;

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }

    if (reflectOut) {
        crc = reflect(crc, 32);
    }

    return crc ^ finalXor;
}