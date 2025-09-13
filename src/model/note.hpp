#pragma once
#include <stdint.h>

enum NoteFlags : uint8_t
{
    NF_Mute = 1 << 0
};

struct Note
{
    uint32_t on,  // tick start
        duration; // ticks

    int16_t micro, // sub-tick (ticks/256), signed
        micro_q8;  // fractional ticks * 1/256 (can be negative; we delay only if >0)

    uint8_t pitch, // 0–127
        vel,       // 0–127
        flags;     // bitset
};
