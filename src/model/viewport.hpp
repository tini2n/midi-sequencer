#pragma once
#include <stdint.h>
#include "note.hpp"

struct Viewport
{
    static constexpr int16_t W = 128, H = 64;

    uint32_t tickStart{0}, tickSpan{2048};
    uint8_t pitchBase{12}, lanes{12}; // rows on 64px
    uint8_t grid{16};

    void clamp(uint32_t maxTicks)
    {
        if (maxTicks)
        {
            if (tickSpan > maxTicks)
                tickSpan = maxTicks;
            if (tickStart + tickSpan > maxTicks)
                tickStart = (maxTicks > tickSpan) ? (maxTicks - tickSpan) : 0;
        }
    }
    void pan_ticks(int32_t dt, uint32_t maxTicks = 0)
    {
        int64_t ns = int64_t(tickStart) + dt;
        if (ns < 0)
            ns = 0;
        tickStart = uint32_t(ns);
        clamp(maxTicks);
    }
    void zoom_ticks(float f, uint32_t max = 0, int anchor_px = W / 2)
    {
        uint32_t old = tickSpan;
        uint32_t ns = uint32_t(tickSpan / f);
        if (ns < 64)
            ns = 64;
        if (ns > 65536)
            ns = 65536;
        tickSpan = ns;
        uint64_t anchorTick = tickStart + (uint64_t)old * anchor_px / W;
        tickStart = (anchorTick > (uint64_t)tickSpan * anchor_px / W)
                        ? uint32_t(anchorTick - (uint64_t)tickSpan * anchor_px / W)
                        : 0;
        clamp(max);
    }
    void pan_pitch(int d)
    {
        int p = pitchBase + d;
        if (p < 0)
            p = 0;
        if (p > 119)
            p = 119;
        pitchBase = (uint8_t)p;
    }
};