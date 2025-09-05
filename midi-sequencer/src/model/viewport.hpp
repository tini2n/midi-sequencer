#pragma once
#include <stdint.h>
#include "note.hpp"

struct Viewport
{
    uint32_t tickStart{0}, tickSpan{2048};
    uint8_t pitchBase{36}, lanes{12}; // rows on 64px
    uint8_t grid{4};

    void pan_ticks(int32_t dt)
    {
        int64_t ns = tickStart;
        ns += dt;
        if (ns < 0)
            ns = 0;
        tickStart = (uint32_t)ns;
    }
    void zoom_ticks(float f)
    {
        uint32_t ns = (uint32_t)(tickSpan / f);
        if (ns < 64)
            ns = 64;
        if (ns > 65536)
            ns = 65536;
        tickSpan = ns;
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

struct Rect
{
    int16_t x, y, w, h;
};

inline bool project(const Viewport &v, const Note &n, Rect &r)
{
    int32_t x0 = (int32_t)((int64_t)(n.on - v.tickStart) * 128 / (int32_t)v.tickSpan);
    int32_t x1 = (int32_t)((int64_t)(n.on + n.duration - v.tickStart) * 128 / (int32_t)v.tickSpan);

    if (x1 <= 0 || x0 >= 128)
        return false;

    if (x0 < 0)
        x0 = 0;

    if (x1 > 128)
        x1 = 128;

    uint8_t rowH = (v.lanes ? (uint8_t)(64 / v.lanes) : 8);

    if (!rowH)
        rowH = 4;

    int16_t lane = (int16_t)n.pitch - (int16_t)v.pitchBase;

    if (lane < 0 || lane >= v.lanes)
        return false;

    int16_t y = 63 - (lane + 1) * rowH + 1;

    if (y < 0)
        y = 0;

    r = {(int16_t)x0, y, (int16_t)((x1 - x0) ? (x1 - x0) : 1), (int16_t)(rowH - 2)};

    return r.w > 0 && r.h > 0;
}
