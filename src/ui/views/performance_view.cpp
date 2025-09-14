#include "performance_view.hpp"

void PerformanceView::draw(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick)
{
    (void)midi;
    (void)pat;
    (void)vp;
    (void)playTick;

    char hud[48];
    const char *modeStr = (st_.mode == PerformanceMode::Keyboard) ? "KB" : (st_.mode == PerformanceMode::Scale) ? "SC"
                                                                                                                : "CHR";
    snprintf(hud, sizeof(hud), "%s CH:%u OC:%d BP:%d", modeStr, st_.channel, st_.octave, (int)pat.tempo);

    PianoRoll::Options o = {};
    o.highlightPitch = st_.lastPitch;
    oled.rollSetOptions(o);                      // uses stripe drawing in roll render :contentReference[oaicite:0]{index=0}
    oled.drawFrame(pat, vp, now, playTick, hud); // HUD bar at top (5x7) :contentReference[oaicite:1]{index=1}
}
