#include "performance_view.hpp"

void PerformanceView::draw(Pattern &pat, Viewport &vp, OledRenderer &oled, MidiIO &midi, uint32_t now, uint32_t playTick)
{
    (void)midi;
    (void)pat;
    (void)vp;
    (void)playTick;

    char hud[48];

    const char *scaleStr = "OFF";
    if (st_.scale == (uint8_t)Scale::Dorian) scaleStr = "Dor";
    else if (st_.scale == (uint8_t)Scale::Lydian) scaleStr = "Lyd";
    // HUD: mode indicator, bpm, octave, scale/fold
    snprintf(hud, sizeof(hud), "PERF BP:%d OC:%d SC:%s%s", (int)pat.tempo, st_.octave, scaleStr, st_.fold?"*":"");

    PianoRoll::Options o = {};
    o.highlightPitch = st_.lastPitch;
    oled.rollSetOptions(o);
    oled.drawFrame(pat, vp, now, playTick, hud);
}
