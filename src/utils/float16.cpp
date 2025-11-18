#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <array>

struct float16_le {
    uint16_t bits_native; // stored in host/native order

    float16_le() : bits_native(0) {}
    explicit float16_le(float f) { assign_from_float(f); }

    // Construct from two bytes that are in little-endian order (buf[0] = LSB)
    static float16_le from_le_bytes(const uint8_t buf[2]) {
        float16_le h;
        h.bits_native = uint16_t(buf[0]) | (uint16_t(buf[1]) << 8);
        return h;
    }

    // Write little-endian bytes into out[0..1]
    void to_le_bytes(uint8_t out[2]) const {
        out[0] = uint8_t(bits_native & 0xFF);
        out[1] = uint8_t((bits_native >> 8) & 0xFF);
    }

    // implicit conversion to float
    operator float() const {
        // bits_native is in host order and holds the IEEE-754 half bits
        uint16_t h = bits_native;
        uint32_t sign = (uint32_t)(h & 0x8000) << 16;
        uint32_t exp  = (h & 0x7C00) >> 10;
        uint32_t frac = (h & 0x03FF);

        uint32_t fbits;
        if (exp == 0) {
            if (frac == 0) {
                // zero
                fbits = sign;
            } else {
                // subnormal -> normalize
                // shift until leading 1 in bit 10 (0x400)
                exp = 1;
                while ((frac & 0x0400) == 0) {
                    frac <<= 1;
                    --exp;
                }
                frac &= 0x03FF;
                uint32_t newexp = exp + (127 - 15);
                fbits = sign | (newexp << 23) | (frac << 13);
            }
        } else if (exp == 0x1F) {
            // inf or NaN
            fbits = sign | 0x7F800000 | (frac << 13);
        } else {
            // normalized
            uint32_t newexp = exp + (127 - 15);
            fbits = sign | (newexp << 23) | (frac << 13);
        }

        float out;
        std::memcpy(&out, &fbits, sizeof(out));
        return out;
    }

    // assign from float (round-to-nearest-even)
    void assign_from_float(float f) {
        uint32_t x;
        std::memcpy(&x, &f, sizeof(x));

        uint32_t sign = (x >> 16) & 0x8000; // top bit -> half sign position
        int32_t exp  = int32_t((x >> 23) & 0xFF) - 127; // unbiased exponent
        uint32_t frac = x & 0x007FFFFF;

        uint16_t h = 0;

        if (exp == 128) {
            // Inf or NaN
            h = uint16_t(sign | 0x7C00u | (frac ? ( (frac >> 13) ? ( (frac >> 13) ) : 1 ) : 0));
        } else if (exp > 15) {
            // Overflow -> set to Inf (could implement max-finite)
            h = uint16_t(sign | 0x7C00u);
        } else if (exp >= -14) {
            // normalized number
            uint16_t newexp = uint16_t(exp + 15);
            uint16_t newfrac = uint16_t(frac >> 13);

            // round to nearest: check bit 12 (first dropped) and sticky bits
            uint32_t round_bit = (frac >> 12) & 1u;
            uint32_t sticky = frac & ((1u << 12) - 1u);
            if (round_bit && (sticky || (newfrac & 1u))) {
                ++newfrac;
                if (newfrac == 0x400) { // overflowed fractional into exponent
                    newfrac = 0;
                    ++newexp;
                    if (newexp == 0x1F) { // overflow to inf
                        newexp = 0x1F;
                        newfrac = 0;
                    }
                }
            }

            h = uint16_t(sign | (newexp << 10) | (newfrac & 0x3FFu));
        } else if (exp >= -24) {
            // subnormal (fits as subnormal half)
            int shift = (-14 - exp);
            uint32_t mant = (frac | 0x00800000u) >> (13 + shift);

            // rounding for subnormal: check bits shifted out
            uint32_t round_mask = (1u << (13 + shift)) - 1u;
            uint32_t round_part = frac & round_mask;
            uint32_t round_bit = (frac >> (12 + shift)) & 1u;
            if (round_bit && (round_part || (mant & 1u))) ++mant;

            h = uint16_t(sign | (mant & 0x3FFu));
        } else {
            // too small -> zero
            h = uint16_t(sign);
        }

        bits_native = h;
    }
};