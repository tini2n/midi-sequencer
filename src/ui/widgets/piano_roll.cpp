#include "piano_roll.hpp"

static inline int32_t xFromTick(uint32_t tick, const Viewport &v)
{
    return PianoRoll::Layout::GRID_X +
           (int32_t)(((int64_t)tick - (int64_t)v.tickStart) * PianoRoll::Layout::GRID_W / (int32_t)v.tickSpan);
}
static inline int16_t yFromPitch(uint8_t pitch, const Viewport &v)
{
    int16_t lane = (int16_t)pitch - (int16_t)v.pitchBase;
    return (int16_t)(PianoRoll::Layout::H - 1 - (lane + 1) * PianoRoll::Layout::LANE_H + 1);
}
static void pitchTextC0(uint8_t p, char out[5])
{
    static const char *N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int8_t oct = int8_t(p / 12);
    snprintf(out, 5, "%s%d", N[p % 12], oct); // C0..B10
}
void PianoRoll::drawGrid(U8G2 &u8g2, const Viewport &v)
{
    const uint8_t steps = 24;
    const int X0 = Layout::GRID_X, X1 = X0 + Layout::GRID_W, H = Layout::H;

    uint32_t step = v.tickStart / steps;

    while (true)
    {
        uint32_t t = step * steps;
        int x = xFromTick(t, v);
        if (x > X1)
            break;
        if (x >= X0)
        {
            if (step % 16 == 0)
            {
                // main bar line
                u8g2.drawVLine(x, 0, H);
                u8g2.drawVLine(x + 1, 0, H);
                // removed this line to avoid double lines at bar 0
            }
            else if (step % 4 == 0)
                u8g2.drawVLine(x, 0, H);
            else
            {
                u8g2.drawPixel(x, 0);
                u8g2.drawPixel(x, H - 1);
            } // 1/8 markers appear as dots
        }
        step++;
    }
    u8g2.drawVLine(X0 - 1, 0, H);
}

void PianoRoll::drawPitch(U8G2 &u8g2, const Viewport &v)
{
    const int H = Layout::H, LW = Layout::LABEL_W, LH = Layout::LANE_H, GX = Layout::GRID_X, GW = Layout::GRID_W;

    u8g2.setFont(u8g2_font_micro_mr);
    uint8_t lanes = H / LH; // ≈9
    uint8_t pb = v.pitchBase > (uint8_t)(127 - lanes) ? (uint8_t)(127 - lanes) : v.pitchBase;

    for (uint8_t row = 0; row < lanes; ++row)
    {
        uint8_t p = pb + row;
        int16_t y0 = H - (row + 1) * LH;

        if (isBlackKey(p))
            u8g2.drawBox(0, y0, LW, LH);
        char txt[5];
        pitchTextC0(p, txt);

        if (isBlackKey(p))
        {
            u8g2.setDrawColor(0);
            u8g2.drawStr(1, y0 + LH - 1, txt);
            u8g2.setDrawColor(1);
        }
        else
            u8g2.drawStr(1, y0 + LH - 1, txt);
            
        // if (row > 0)
            u8g2.drawHLine(GX, y0, GW);
    }
}

void PianoRoll::drawNotes(U8G2 &u8g2, const Track &t, const Viewport &v)
{
    const int GX = Layout::GRID_X, GW = Layout::GRID_W;
    const int LH = Layout::LANE_H, H = Layout::H;
    const int lanes = H / LH;

    for (const auto &n : t.notes)
    {
        // clip by pitch rows
        int16_t lane = int16_t(n.pitch) - int16_t(v.pitchBase);
        if (lane < 0 || lane >= v.lanes)
            continue;

        int32_t x0 = xFromTick(n.on, v);
        int32_t x1 = xFromTick(n.on + n.duration, v);

        if (x1 <= GX || x0 >= GX + GW)
            continue;
        if (x0 < GX)
            x0 = GX;
        if (x1 > GX + GW)
            x1 = GX + GW;

        int16_t y = yFromPitch(n.pitch, v);
        int16_t w = (int16_t)((x1 - x0) > 0 ? (x1 - x0) : 1);
        int16_t h = (int16_t)(LH - 1);

        if (n.vel < 64)
            u8g2.drawFrame(x0, y, w, h);
        else if (n.vel < 100)
            for (int xx = x0; xx < x0 + w; xx += 2)
                u8g2.drawVLine(xx, y, h);
        else
            u8g2.drawBox(x0, y, w, h);
    }
}

void PianoRoll::drawPlayhead(U8G2 &u8g2, const Viewport &v, uint32_t playTick)
{
    const int GX = Layout::GRID_X, GW = Layout::GRID_W;
    int x = xFromTick(playTick, v);
    if (x >= GX && x < GX + GW)
        u8g2.drawVLine(x, 0, Layout::H);
}

void PianoRoll::render(U8G2 &u8g2, const Track &t, const Viewport &v, uint32_t playTick)
{
    drawPitch(u8g2, v);
    drawGrid(u8g2, v);
    drawNotes(u8g2, t, v);
    drawPlayhead(u8g2, v, playTick);
}
