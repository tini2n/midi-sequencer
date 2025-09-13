#pragma once
#include <stdint.h>

namespace Scale
{
    static inline uint16_t majorMask(uint8_t root)
    {
        const uint16_t M = 0b101011010101;
        return uint16_t(((M << root) | (M >> (12 - root))) & 0x0FFF);
    }
    static inline uint16_t minorMask(uint8_t root)
    {
        const uint16_t N = 0b101101011010;
        return uint16_t(((N << root) | (N >> (12 - root))) & 0x0FFF);
    }
    static inline bool inMask(uint8_t pc, uint16_t mask) { return (mask >> (pc % 12)) & 1u; }

}
