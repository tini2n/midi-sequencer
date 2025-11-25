#include "piano_roll.hpp"

int32_t PianoRoll::xFromTick(uint32_t tick, const Viewport &v)
{
    return PianoRoll::Layout::GRID_X +
           (int32_t)(((int64_t)tick - (int64_t)v.tickStart) * PianoRoll::Layout::GRID_W / (int32_t)v.tickSpan);
}
int16_t PianoRoll::yFromPitch(uint8_t pitch, const Viewport &v)
{
    int16_t lane = (int16_t)pitch - (int16_t)v.pitchBase;
    return (int16_t)(PianoRoll::Layout::H - 1 - (lane + 1) * PianoRoll::Layout::LANE_H + 1);
}
void PianoRoll::drawGrid(U8G2 &u8g2, const Viewport &v)
{
    const uint8_t steps = 24; // 1/16 @ PPQN=96
    const int X0 = Layout::GRID_X, X1 = X0 + Layout::GRID_W, H = Layout::H;

    uint32_t step = v.tickStart / steps;

    for (;; step++)
    {
        int x = xFromTick(step * steps, v);

        if (x > X1)
            break;

        if (x < X0)
            continue;

        if (step % 16 == 0) // bar
        {
            for (int y = 0; y < H; y += 1)
                u8g2.drawPixel(x, y);
        }
        else if (step % 4 == 0) // beat
        {
            for (int y = 1; y < H; y += 3)
                u8g2.drawPixel(x, y);
        }
        // else // sixteenth step dots
        // {
        //     // Minimal hint on sixteenths: bottom end-cap dot
        //     u8g2.drawPixel(x, H - 1);
        // }
    }
    u8g2.drawVLine(X0, 0, H);
}

void PianoRoll::drawLanes(U8G2 &u8g2, const Viewport &v)
{
    const int H = Layout::H, LW = Layout::LABEL_W, LH = Layout::LANE_H;
    (void)LW; // May be unused depending on drawing logic

    // u8g2.setFont(u8g2_font_minimal3x3_tu);
    u8g2.setFont(u8g2_font_u8glib_4_tf);
    uint8_t lanes = H / LH; // ≈9

    uint8_t pb = v.pitchBase;

    if (pb < options_.pMin)
        pb = options_.pMax;

    if (pb > uint8_t(options_.pMax - (lanes ? lanes : 1)))
        pb = uint8_t(options_.pMax - (lanes ? lanes : 1));

    for (uint8_t row = 0; row < lanes; ++row)
    {
        uint8_t p = pb + row;
        int16_t y0 = H - (row + 1) * LH;

        if (p == (uint8_t)options_.highlightPitch)
        { // 2px stripe near label
            u8g2.drawBox(Layout::LABEL_W - 2, y0, 2, LH);
        }

        // Invert coloring: white keys filled, black keys not filled
        if (!isBlackKey(p))
            u8g2.drawBox(0, y0, LW, LH);

        char txt[5];
        drawPitchText(p, txt);

        if (!isBlackKey(p))
        {
            // White key area is filled, so draw inverted text for contrast
            u8g2.setDrawColor(0);
            u8g2.drawStr(1, y0 + LH - 1, txt);
            u8g2.setDrawColor(255);
        }
        else
            u8g2.drawStr(1, y0 + LH - 1, txt);

        // if (row > 0)
        // u8g2.drawHLine(GX, y0, GW);
    }
}

void PianoRoll::drawNotes(U8G2 &u8g2, const Track &t, const Viewport &v)
{
    const int GX = Layout::GRID_X, GX1 = GX + Layout::GRID_W, H = Layout::H, LH = Layout::LANE_H;
    const int lanes = H / LH;
    
    // Early bailout for empty track
    if (t.notes.empty())
        return;
    
    // Limit iterations for performance - if we have too many notes, only draw visible ones
    uint16_t notesDrawn = 0;
    const uint16_t MAX_NOTES_PER_FRAME = 32; // Safety limit

    for (const auto &n : t.notes)
    {
        // Safety: prevent runaway rendering
        if (notesDrawn >= MAX_NOTES_PER_FRAME)
            break;
        
        // Early exit optimization: IF track.notes sorted by time, we can stop early
        // Call track.sortByTime() after edits for best performance
        if (n.on > v.tickStart + v.tickSpan)
            break; // All remaining notes are after viewport
            
        // Fast viewport culling: check time window first (cheapest)
        if (n.on + n.duration < v.tickStart)
            continue; // Note ends before viewport
            
        // clip by pitch rows
        if (n.pitch < options_.pMin || n.pitch > options_.pMax)
            continue;

        int16_t lane = int16_t(n.pitch) - int16_t(v.pitchBase);
        if (lane < 0 || lane >= lanes)
            continue;

        int32_t x0 = xFromTick(n.on, v);
        int32_t x1 = xFromTick(n.on + n.duration, v);

        if (x1 <= GX || x0 >= GX1)
            continue;
        if (x0 < GX)
            x0 = GX;
        if (x1 > GX1)
            x1 = GX1;

        int16_t y = yFromPitch(n.pitch, v);
        int16_t w = (int16_t)((x1 - x0) > 0 ? (x1 - x0) : 1);
        int16_t h = (int16_t)(LH - 1);

        if (n.vel < 64)
            drawLightFill(u8g2, x0, y, w, h);
        else if (n.vel < 100)
            drawMediumFill(u8g2, x0, y, w, h);
        else
            u8g2.drawBox(x0, y, w, h); // Full velocity
            
        notesDrawn++;
    }
}

void PianoRoll::drawPlayhead(U8G2 &u8g2, const Viewport &v, uint32_t playTick)
{
    int x = xFromTick(playTick, v);
    if (x >= Layout::GRID_X && x < Layout::GRID_X + Layout::GRID_W)
        u8g2.drawVLine(x, 0, Layout::H);
}

void PianoRoll::render(U8G2 &u8g2, const Track &t, const Viewport &v, uint32_t playTick)
{
    drawLanes(u8g2, v);
    drawGrid(u8g2, v);
    drawNotes(u8g2, t, v);
    drawPlayhead(u8g2, v, playTick);
}
