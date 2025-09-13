#include "renderer_oled.hpp"

bool OledRenderer::begin()
{
    u8g2_.begin();
    u8g2_.setContrast(80);
    u8g2_.setBusClock(1000000);
    return true;
}
uint32_t OledRenderer::drawFrame(const Pattern &p, const Viewport &v, uint32_t now, uint32_t playTick, const char *hud)
{
    uint32_t t0 = now;
    u8g2_.firstPage();
    do
    {
        pianoRoll_.render(u8g2_, p.track, v, playTick);
        // if (hud)
        // {
        //     u8g2_.setFont(u8g2_font_5x7_tf);
        //     u8g2_.drawBox(0, 0, 128, 8);
        //     u8g2_.setDrawColor(0);
        //     u8g2_.drawStr(1, 7, hud);
        //     u8g2_.setDrawColor(1);
        // }
    } while (u8g2_.nextPage());
    return (uint32_t)(micros() - t0);
}
