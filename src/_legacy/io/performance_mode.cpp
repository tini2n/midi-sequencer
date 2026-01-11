#include "performance_mode.hpp"
#include "model/scale.hpp"

void PerformanceKBMode::onButtonDown(uint8_t btn, MidiIO &midi, uint8_t ch, void *context)
{
    // context can optionally hold lastPitch pointer
    int *lastPitchOpt = static_cast<int *>(context);
    noteOn(btn, midi, ch, lastPitchOpt);
}

void PerformanceKBMode::onButtonUp(uint8_t btn, MidiIO &midi, uint8_t ch, void *context)
{
    (void)context;
    noteOff(btn, midi, ch);
}

void PerformanceKBMode::onActivate()
{
    Serial.println("[PerformanceMode] Activated");
    reset();
}

void PerformanceKBMode::onDeactivate()
{
    Serial.println("[PerformanceMode] Deactivated");
}

void PerformanceKBMode::setRoot(uint8_t r)
{
    root_ = r % 12;
    computeBottomOffsets();
}

void PerformanceKBMode::setOctave(int8_t o)
{
    if (o < 0)
        o = 0;
    if (o > 10)
        o = 10;
    oct_ = o;
}

void PerformanceKBMode::setScale(Scale s)
{
    scale_ = s;
}

void PerformanceKBMode::reset()
{
    for (int i = 0; i < 16; i++)
    {
        pressed_[i] = false;
        pitch_[i] = -1;
    }
}

void PerformanceKBMode::computeBottomOffsets()
{
    // map semitone→natural letter index (C=0..B=6), sharps map down to preceding natural
    static const uint8_t natIdx[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static const uint8_t gapsC[7] = {2, 2, 1, 2, 2, 2, 1}; // C,D,E,F,G,A,B intervals
    uint8_t li = natIdx[root_];
    bot_[0] = 0;
    uint8_t acc = 0;
    for (int i = 0; i < 7; i++)
    {
        acc += gapsC[(i + li) % 7];
        bot_[i + 1] = acc;
    } // offsets for 8 naturals
}

int PerformanceKBMode::btnToPitch(uint8_t btn) const
{
    int base = 12 * oct_ + root_;

    // If scale folding is enabled and a scale is selected, map 0..15 to scale degrees
    if (fold_ && scale_ != Scale::None)
    {
        // In fold mode, order buttons by physical rows: bottom row first, then top row.
        // Current logical mapping: top row -> 0..7, bottom row -> 8..15.
        // We swap them so bottom row is indices 0..7 and top row 8..15 for fold mapping.
        uint8_t idx = (btn < 8) ? (btn + 8) : (btn - 8);
        // Map 16 buttons across scale degrees and octaves: degrees 0..6 per octave.
        uint8_t deg = idx % 7;     // within one octave
        uint8_t octUp = (idx / 7); // 0,1,2
        uint8_t semi = scale::degreeSemitone(scale_, deg) + 12 * octUp;
        return clamp(base + semi);
    }

    // Original natural + black-gaps mapping
    if (btn >= 8)
    {
        uint8_t k = btn - 8;
        int p = clamp(base + bot_[k]);
        // Filter if a scale is selected (when not folding)
        return (scale_ == Scale::None || scale::contains(scale_, root_, (uint8_t)p)) ? p : -1;
    } // q..i naturals

    if (btn == 0)
        return -1; // no black before first

    uint8_t g = btn - 1;

    if (g >= 7)
        return -1; // gap 0..6

    uint8_t diff = bot_[g + 1] - bot_[g];

    if (diff == 2)
    {
        int p = clamp(base + bot_[g] + 1); // whole-tone gap → black
        return (scale_ == Scale::None || scale::contains(scale_, root_, (uint8_t)p)) ? p : -1;
    }

    return -1;
}

void PerformanceKBMode::noteOn(int btn, MidiIO &midi, uint8_t ch, int *lastPitchOpt)
{
    int p = btnToPitch(btn);
    if (p < 0)
        return;
    if (pressed_[btn])
        return; // no double-trigs
    Serial.printf("[PerformanceMode] Note ON %d (btn %d) ch%u v%u\n", p, btn, ch, vel_);
    pressed_[btn] = true;
    pitch_[btn] = p;
    midi.send({ch, (uint8_t)p, vel_, true, 0});
    if (rec_ && tx_ && tx_->isRunning() && rec_->isArmed())
        rec_->onLiveNoteOn((uint8_t)p, vel_, tx_->playTick());
    if (lastPitchOpt)
        *lastPitchOpt = p;
}

void PerformanceKBMode::noteOff(int btn, MidiIO &midi, uint8_t ch)
{
    if (!pressed_[btn])
        return;
    int p = pitch_[btn];
    if (p < 0 || p > 127)
    {
        // Guard against invalid stored pitch; skip sending malformed MIDI
        Serial.printf("[PerformanceMode] Note OFF (invalid pitch %d) (btn %d) ch%u\n", p, btn, ch);
    }
    else
    {
        Serial.printf("[PerformanceMode] Note OFF %d (btn %d) ch%u\n", p, btn, ch);
        midi.send({ch, (uint8_t)p, 0, false, 0});
    }
    if (rec_ && tx_ && rec_->isArmed())
        rec_->onLiveNoteOff((uint8_t)p, tx_->playTick());
    pressed_[btn] = false;
    pitch_[btn] = -1;
}
