#pragma once
#include <stdint.h>
#include "note.hpp"

struct Viewport
{
    static constexpr int16_t W = 128, H = 64;

    uint32_t tickStart{0}, tickSpan{2048};
    uint8_t pitchBase{36}, lanes{12}; // rows on 64px
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

struct Rect
{
    int16_t x, y, w, h;
};

inline bool project(const Viewport &v, const Note &n, Rect &r)
{
    const int32_t W = Viewport::W, H = Viewport::H;

    int64_t on256 = (int64_t)n.on * 256 + n.micro;
    int64_t en256 = on256 + (int64_t)n.duration * 256;

    int64_t st = (on256 - (int64_t)v.tickStart * 256);

    int32_t x0 = (int32_t)(st * W / ((int64_t)v.tickSpan * 256));
    int32_t x1 = (int32_t)((en256 - (int64_t)v.tickStart * 256) * W / ((int64_t)v.tickSpan * 256));

    if (x1 <= 0 || x0 >= W)
        return false;

    if (x0 < 0)
        x0 = 0;

    if (x1 > W)
        x1 = W;

    uint8_t rowH = v.lanes ? (uint8_t)(H / v.lanes) : 8;

    if (!rowH)
        rowH = 4;

    int16_t lane = int16_t(n.pitch) - int16_t(v.pitchBase);

    if (lane < 0 || lane >= v.lanes)
        return false;

    int16_t y = (H - 1) - (lane + 1) * rowH + 1;
    if (y < 0)
        y = 0;

    r = {(int16_t)x0, y, (int16_t)((x1 - x0) ? (x1 - x0) : 1), (int16_t)(rowH - 2)};

    return r.w > 0 && r.h > 0;
}
