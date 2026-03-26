#pragma once
#include <stdint.h>

// Navigation window over the piano-roll display.
// Tracks which slice of ticks and which pitch range is currently visible.
// Read by the renderer; written by encoder/keyboard input handlers.
// Pure UI state — no model or MIDI dependency.
struct Viewport {
    static constexpr int16_t W = 256; // display width in pixels (SSD1322)
    static constexpr int16_t H = 64;  // display height in pixels

    uint32_t tickStart{0};    // first tick visible on screen
    uint32_t tickSpan{2048};  // number of ticks across the full width
    uint8_t  pitchBase{12};   // MIDI note at the bottom of the display (C0)

    // pixels per tick — used by renderer to convert tick → x pixel
    float pixelsPerTick() const { return float(W) / float(tickSpan); }

    // Clamp viewport so it doesn't scroll past the end of the pattern.
    void clamp(uint32_t maxTicks) {
        if (!maxTicks) return;
        if (tickSpan > maxTicks)
            tickSpan = maxTicks;
        if (tickStart + tickSpan > maxTicks)
            tickStart = (maxTicks > tickSpan) ? (maxTicks - tickSpan) : 0;
    }

    // Scroll horizontally by dt ticks (negative = left, positive = right).
    void pan_ticks(int32_t dt, uint32_t maxTicks = 0) {
        int64_t ns = int64_t(tickStart) + dt;
        if (ns < 0) ns = 0;
        tickStart = uint32_t(ns);
        clamp(maxTicks);
    }

    // Zoom time axis by factor f around anchor_px (default: screen centre).
    // f > 1 = zoom in (fewer ticks visible), f < 1 = zoom out (more ticks).
    void zoom_ticks(float f, uint32_t max = 0, int anchor_px = W / 2) {
        uint32_t old = tickSpan;
        uint32_t ns  = uint32_t(tickSpan / f);
        if (ns < 64)    ns = 64;
        if (ns > 65536) ns = 65536;
        tickSpan = ns;
        uint64_t anchorTick = tickStart + (uint64_t)old * anchor_px / W;
        tickStart = (anchorTick > (uint64_t)tickSpan * anchor_px / W)
                        ? uint32_t(anchorTick - (uint64_t)tickSpan * anchor_px / W)
                        : 0;
        clamp(max);
    }

    // Scroll pitch axis by d semitones (negative = down, positive = up).
    void pan_pitch(int d) {
        int p = pitchBase + d;
        if (p < 0)   p = 0;
        if (p > 119) p = 119;
        pitchBase = uint8_t(p);
    }
};
