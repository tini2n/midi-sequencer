#include "renderer_oled.hpp"

bool OledRenderer::begin()
{
    u8g2_.begin();
    
    u8g2_.setContrast(100);      // 0..255
    u8g2_.setBusClock(16000000); // 8–24 MHz; tune later
    
    return true;
}
uint32_t OledRenderer::drawFrame(const Pattern &p, const Viewport &v, uint32_t now, uint32_t playTick, const char *hud)
{
    uint32_t t0 = now;
    
    // U8g2 page loop - each page is rendered separately
    // This is hardware limitation, but we minimize work per page
    u8g2_.firstPage();
    do
    {
        // Piano roll rendering (optimized with viewport culling)
        pianoRoll_.render(u8g2_, p.track, v, playTick);
        
        // HUD overlay (only if provided)
        if (hud)
        {
            u8g2_.setFont(u8g2_font_5x7_tf);
            u8g2_.drawBox(0, 0, 256, 8);
            u8g2_.setDrawColor(0);
            u8g2_.drawStr(1, 7, hud);
            u8g2_.setDrawColor(255);
        }
    } while (u8g2_.nextPage());
    
    return (uint32_t)(micros() - t0);
}
