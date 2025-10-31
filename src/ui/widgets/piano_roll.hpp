#pragma once
#include "U8g2lib.h"
#include "model/track.hpp"
#include "model/viewport.hpp"

struct PianoRoll
{
    struct Layout
    {
        static constexpr uint16_t W = 256;
        static constexpr uint8_t H = 64, LABEL_W = 14, GRID_X = 14, GRID_W = 242, LANE_H = 6;
    };
    struct Options
    {
        // TODO: Make filteration of notes in pitch range
        uint8_t pMin = 0, pMax = 127;
        int16_t highlightPitch=-1; 
    } options_;

    void setOptions(const Options &o) { options_ = o; }
    void render(U8G2 &u8g2, const Track &t, const Viewport &v, uint32_t tick = 0);

private:
    static int32_t xFromTick(uint32_t tick, const Viewport &v);
    static int16_t yFromPitch(uint8_t pitch, const Viewport &v);

    void drawGrid(U8G2 &u8g2, const Viewport &v);
    void drawLanes(U8G2 &u8g2, const Viewport &v);
    void drawNotes(U8G2 &u8g2, const Track &t, const Viewport &v);
    void drawPlayhead(U8G2 &u8g2, const Viewport &v, uint32_t playTick);

    // Velocity rendering helpers
    inline void drawLightFill(U8G2 &u8g2, int x, int y, int w, int h)
    {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x + (yy & 1); xx < x + w; xx += 2)
                u8g2.drawPixel(xx, yy);
    }
    inline void drawMediumFill(U8G2 &u8g2, int x, int y, int w, int h)
    {
        for (int xx = x; xx < x + w; xx += 2)
            u8g2.drawVLine(xx, y, h);
    }

    // Gray line drawing helpers
    inline void drawGrayVLine(U8G2 &u8g2, int x, int y, int h, uint8_t grayLevel)
    {
        u8g2.setDrawColor(grayLevel);
        u8g2.drawVLine(x, y, h);
        u8g2.setDrawColor(255); // Reset to white
    }
    inline void drawGrayHLine(U8G2 &u8g2, int x, int y, int w, uint8_t grayLevel)
    {
        u8g2.setDrawColor(grayLevel);
        u8g2.drawHLine(x, y, w);
        u8g2.setDrawColor(255); // Reset to white
    }

    // Pitch rendering helpers
    static bool isBlackKey(uint8_t midi)
    {
        uint8_t k = midi % 12;
        return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
    }
    static void drawPitchText(uint8_t p, char out[5])
    {
        static const char *N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int8_t oct = int8_t(p / 12);
        snprintf(out, 5, "%s%d", N[p % 12], oct);
    }
};
