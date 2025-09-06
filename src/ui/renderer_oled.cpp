#include "renderer_oled.hpp"

bool OledRenderer::begin()
{
    u8g2_.begin();
    u8g2_.setBusClock(1000000);
    return true;
}
void OledRenderer::drawGrid(const Viewport &v)
{
    // vertical grid (8 columns)
    for (int i = 0; i <= 8; i++)
    {
        int x = i * 16;
        u8g2_.drawVLine(x, 0, 64);
    }
    // horizontal lanes
    uint8_t rowH = (v.lanes ? (uint8_t)(64 / v.lanes) : 8);
    if (!rowH)
        rowH = 4;
    for (int y = rowH; y < 64; y += rowH)
        u8g2_.drawHLine(0, 63 - y, 128);
}
uint32_t OledRenderer::drawFrame(const Pattern &p, const Viewport &v, uint32_t now, const char *hud)
{
    uint32_t t0 = now;
    u8g2_.firstPage();
    do
    {
        drawGrid(v);
        // notes
        Rect r;
        for (const auto &n : p.track.notes)
            if (project(v, n, r))
                u8g2_.drawBox(r.x, r.y, r.w, r.h);
                
        if (hud)
        {
            u8g2_.setFont(u8g2_font_5x7_tf);
            u8g2_.drawBox(0, 0, 128, 8);
            u8g2_.setDrawColor(0);
            u8g2_.drawStr(1, 7, hud);
            u8g2_.setDrawColor(1);
        }
    } while (u8g2_.nextPage());
    return (uint32_t)(micros() - t0);
}
