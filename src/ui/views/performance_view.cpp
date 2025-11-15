#include "performance_view.hpp"

void PerformanceView::onEncoderRotation(const EncoderRotationEvent& event)
{
    switch (event.encoderId)
    {
        case 0: // ENC1: Root note
        {
            int newRoot = (int)st_.root + event.delta;
            // Wrap around 0-11 for chromatic scale
            while (newRoot < 0) newRoot += 12;
            while (newRoot >= 12) newRoot -= 12;
            setRoot((uint8_t)newRoot);
            Serial.printf("[PerformanceView] ENC1 Root: %d\n", newRoot);
            break;
        }
        case 1: // ENC2: Octave
        {
            int newOct = st_.octave + event.delta;
            if (newOct < 0) newOct = 0;
            if (newOct > 10) newOct = 10;
            setOctave((int8_t)newOct);
            Serial.printf("[PerformanceView] ENC2 Octave: %d\n", newOct);
            break;
        }
        case 2: // ENC3: Scale selection
        {
            int scaleIdx = (int)st_.scale + event.delta;
            // Wrap through Scale enum (None=0, Dorian=1, Lydian=2)
            if (scaleIdx < 0) scaleIdx = 2;
            if (scaleIdx > 2) scaleIdx = 0;
            setScale((Scale)scaleIdx);
            Serial.printf("[PerformanceView] ENC3 Scale: %d\n", scaleIdx);
            break;
        }
        case 3: // ENC4: Reserved (velocity, etc.)
        {
            Serial.printf("[PerformanceView] ENC4 delta: %d (not assigned)\n", event.delta);
            break;
        }
        case 4: // ENC5: Reserved
        case 5: // ENC6: Reserved
        case 6: // ENC7: Reserved
        case 7: // ENC8: Reserved
        {
            Serial.printf("[PerformanceView] ENC%d delta: %d (not assigned)\n", 
                         event.encoderId + 1, event.delta);
            break;
        }
    }
}

void PerformanceView::onEncoderButton(const EncoderButtonEvent& event)
{
    if (event.pressed)
    {
        switch (event.encoderId)
        {
            case 0: // ENC1 SW: Reset root to C
                setRoot(0);
                Serial.println("[PerformanceView] ENC1 SW: Reset root to C");
                break;
            case 1: // ENC2 SW: Reset octave to 4
                setOctave(4);
                Serial.println("[PerformanceView] ENC2 SW: Reset octave to 4");
                break;
            case 2: // ENC3 SW: Toggle fold mode
                setFold(!st_.fold);
                Serial.printf("[PerformanceView] ENC3 SW: Fold %s\n", st_.fold ? "ON" : "OFF");
                break;
            case 3: // ENC4 SW: Reserved
            case 4: // ENC5 SW: Reserved
            case 5: // ENC6 SW: Reserved
            case 6: // ENC7 SW: Reserved
            case 7: // ENC8 SW: Reserved
                Serial.printf("[PerformanceView] ENC%d SW pressed (not assigned)\n", 
                             event.encoderId + 1);
                break;
        }
    }
}

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
