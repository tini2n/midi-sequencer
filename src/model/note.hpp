#pragma once
#include <stdint.h>

struct Note
{
    uint32_t on;       // tick start
    uint32_t duration; // ticks
    int16_t micro;     // sub-tick (ticks/256), signed
    uint8_t pitch;     // 0–127
    uint8_t vel;       // 0–127
    uint8_t flags;     // bitset
};
