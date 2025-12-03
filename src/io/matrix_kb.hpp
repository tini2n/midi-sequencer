#pragma once
#include <Arduino.h>
#include "core/midi_io.hpp"
#include "io/pcf8575.hpp"
#include "core/runloop.hpp"
#include "core/transport.hpp"
#include "engine/record_engine.hpp"
#include "model/scale.hpp"

class MatrixKB
{
public:
    struct Config
    {
        uint8_t address = 0x20; // I2C address of PCF8575
        uint8_t rows[3] = {10, 11, 12};  // Row drive pins
        uint8_t cols[8] = {0, 1, 2, 3, 4, 5, 6, 7};  // Column sense pins
        uint8_t rowTop = 1, rowBot = 2, rowCtl = 0;  // Row indices
        uint32_t debounce_us = 5000;  // Increased from 3ms to 5ms for stability
    };
    bool begin(const Config &c, uint8_t root = 0, int8_t octave = 4, uint8_t vel = 100)
    {
        cfg_ = c;
        pcf_.begin(cfg_.address);
        setRoot(root);
        setOctave(octave);
        vel_ = vel;
        reset();
        // default all pins high (inputs)
        pcf_.write(0xFFFF);
        return true;
    }

    void setRoot(uint8_t r)
    {
        root_ = r % 12;
        computeBottomOffsets();
    }
    void setOctave(int8_t o)
    {
        if (o < 0)
            o = 0;
        if (o > 10)
            o = 10;
        oct_ = o;
    }
    void setVelocity(uint8_t v) { vel_ = v; }
    void setScale(Scale s) { scale_ = s; }
    void setFold(bool f) { fold_ = f; }
    void reset()
    {
        for (int i = 0; i < 16; i++)
        {
            pressed_[i] = false;
            pitch_[i] = -1;
            debUntil_[i] = 0;
        }
    }

    // Attach external systems for control events and recording hooks
    void attach(RunLoop *rl, RecordEngine *rec, Transport *tx)
    {
        rl_ = rl;
        rec_ = rec;
        tx_ = tx;
    }

    // Attach view manager for view switching controls
    void attachViewManager(class ViewManager *vm)
    {
        vm_ = vm;
    }

    void poll(MidiIO &midi, uint8_t ch, int *lastPitchOpt = nullptr)
    {
        uint32_t scanStart = micros();
        
        // Throttle to max 200Hz (5ms interval) to reduce I2C bus load
        if ((int32_t)(scanStart - lastScanUs_) < 5000) {
            return;
        }
        lastScanUs_ = scanStart;
        
        // Scan each row
        for (uint8_t r = 0; r < 3; r++)
        {
            // Drive row low, others high
            if (!driveRow(r)) {
                continue;
            }
            
            // Wait for signals to settle (matrix RC time + PCF8575 propagation)
            delayMicroseconds(200);
            
            // Read column states
            uint16_t pins = 0xFFFF;
            if (!pcf_.read(pins)) {
                continue;
            }
            
            // Process each column
            for (uint8_t c = 0; c < 8; c++)
            {
                // Take a fresh timestamp per key evaluation
                uint32_t now = micros();
                
                bool down = ((pins >> cfg_.cols[c]) & 1) == 0;
                int btn = rowToBtn(r, c);
                if (btn < 0)
                    continue;
                    
                // Debouncing
                uint32_t &deb = (r == cfg_.rowCtl) ? debCtlUntil_[c] : debUntil_[btn];
                if ((int32_t)(now - deb) < 0) {
                    continue;
                }
                
                bool last = lastDown_[r][c];
                if (down == last) {
                    continue;
                }
                
                // State changed
                lastDown_[r][c] = down;
                deb = now + cfg_.debounce_us;
                
                if (r == cfg_.rowCtl)
                {
                    onControl(c, down);
                    continue;
                }
                
                // Musical keys
                if (down)
                {
                    noteOn(btn, midi, ch, lastPitchOpt);
                }
                else
                {
                    noteOff(btn, midi, ch);
                }
            }
        }
        
        // Restore all pins high (inputs)
        pcf_.write(0xFFFF);
    }

private:
    PCF8575 pcf_;
    Config cfg_;
    uint32_t lastScanUs_{0};

    bool driveRow(uint8_t r)
    {
        uint16_t v = 0xFFFF;
        v &= ~(1u << cfg_.rows[r]); // Drive active row low
        return pcf_.write(v);
    }

    int rowToBtn(uint8_t r, uint8_t c) const
    {
        if (r == cfg_.rowTop)
            return c; // 0..7  (top: digits)
        if (r == cfg_.rowBot)
            return 8 + c; // 8..15 (bottom: letters)
        if (r == cfg_.rowCtl)
            return 16 + c; // control buttons (optional)
        return -1;
    }

    void onControl(uint8_t c, bool down);

    void computeBottomOffsets()
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

    int btnToPitch(uint8_t btn) const
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
            uint8_t deg = idx % 7;       // within one octave
            uint8_t octUp = (idx / 7);   // 0,1,2
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
    static int clamp(int p) { return p < 0 ? 0 : (p > 127 ? 127 : p); }

    void pitchName(uint8_t p, char *buf, size_t bufLen) const
    {
        static const char *names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        if (p > 127 || bufLen < 3)
        {
            if (bufLen > 0)
                buf[0] = '\0';
            return;
        }
        uint8_t n = p % 12;
        int8_t o = (p / 12) - 1;
        snprintf(buf, bufLen, "%s%d", names[n], o);
    }

    void noteOn(int btn, MidiIO &midi, uint8_t ch, int *lastPitchOpt)
    {
        int p = btnToPitch(btn);
        if (p < 0)
            return;
        if (pressed_[btn])
            return; // no double-trigs
        Serial.printf("Note ON %d (btn %d) ch%u v%u\n", p, btn, ch, vel_);
        pressed_[btn] = true;
        pitch_[btn] = p;
        midi.send({ch, (uint8_t)p, vel_, true, 0});
        if (rec_ && tx_ && tx_->isRunning() && rec_->isArmed())
            rec_->onLiveNoteOn((uint8_t)p, vel_, tx_->playTick());
        if (lastPitchOpt)
            *lastPitchOpt = p;
    }

    void noteOff(int btn, MidiIO &midi, uint8_t ch)
    {
        if (!pressed_[btn])
            return;
        int p = pitch_[btn];
        if (p < 0 || p > 127)
        {
            // Guard against invalid stored pitch; skip sending malformed MIDI
            Serial.printf("Note OFF (invalid pitch %d) (btn %d) ch%u\n", p, btn, ch);
        }
        else
        {
            Serial.printf("Note OFF %d (btn %d) ch%u\n", p, btn, ch);
            midi.send({ch, (uint8_t)p, 0, false, 0});
        }
        if (rec_ && tx_ && rec_->isArmed())
            rec_->onLiveNoteOff((uint8_t)p, tx_->playTick());
        pressed_[btn] = false;
        pitch_[btn] = -1;
    }

    bool lastDown_[3][8] = {};   // last read state per row/col
    uint32_t debUntil_[16] = {}; // per-button debounce time (musical buttons)
    uint32_t debCtlUntil_[8] = {}; // debounce for control row buttons
    bool pressed_[16] = {};      // per-button pressed state
    int16_t pitch_[16] = {};     // per-button pitch (or -1)

    uint8_t root_{0}, oct_{4}, vel_{100};
    Scale scale_{Scale::None};
    bool fold_{false};
    uint8_t bot_[8] = {}; // bottom row natural note offsets (from root)
    // External systems
    RunLoop *rl_{nullptr};
    RecordEngine *rec_{nullptr};
    Transport *tx_{nullptr};
    class ViewManager *vm_{nullptr};
};