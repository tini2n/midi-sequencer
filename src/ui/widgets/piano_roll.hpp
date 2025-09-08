#pragma once
#include "U8g2lib.h"
#include "model/track.hpp"
#include "model/viewport.hpp"

struct PianoRoll
{
    struct Layout
    {
        static constexpr uint8_t W = 128, H = 64, LABEL_W = 14, GRID_X = 14, GRID_W = 114, LANE_H = 7;
    };
    void render(U8G2 &u8g2, const Track &t, const Viewport &v, uint32_t tick = 0);

private:
    static bool isBlackKey(uint8_t midi)
    {
        uint8_t k = midi % 12;
        return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
    }

    void drawGrid(U8G2 &u8g2, const Viewport &v);
    void drawPitch(U8G2 &u8g2, const Viewport &v);
    void drawNotes(U8G2 &u8g2, const Track &t, const Viewport &v);
    void drawPlayhead(U8G2 &u8g2, const Viewport &v, uint32_t playTick);
};
