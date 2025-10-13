#pragma once
#include <Arduino.h>
#include "core/midi_io.hpp"
#include "io/pcf8575.hpp"
#include "core/runloop.hpp"
#include "core/transport.hpp"
#include "engine/record_engine.hpp"

class MatrixKB
{
public:
    struct Config
    {
        uint8_t address = 0x20; // I2C address of PCF8575
        uint8_t rows[3] = {10, 11, 12};
        uint8_t cols[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        uint8_t rowTop = 1, rowBot = 2, rowCtl = 0;
        uint32_t debounce_us = 3000;
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

    void poll(MidiIO &midi, uint8_t ch, int *lastPitchOpt = nullptr)
    {
        uint32_t now = micros();
        for (uint8_t r = 0; r < 3; r++)
        {
            driveRow(r);
            // Allow signals to settle like in the smoke test (~80us)
            delayMicroseconds(80);
            uint16_t pins = 0xFFFF;
            if (!pcf_.read(pins))
            {
                // If read fails, skip this row to avoid false triggers
                continue;
            }
            // columns pressed = 0
            for (uint8_t c = 0; c < 8; c++)
            {
                bool down = ((pins >> cfg_.cols[c]) & 1) == 0;
                int btn = rowToBtn(r, c);
                if (btn < 0)
                    continue;
                if (now < debUntil_[btn])
                    continue;
                bool last = lastDown_[r][c];
                if (down == last)
                    continue;
                lastDown_[r][c] = down;
                debUntil_[btn] = now + cfg_.debounce_us;
                if (r == cfg_.rowCtl)
                {
                    onControl(c, down);
                    continue;
                }
                // musical rows: emit note on at press, note off at release
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
    }

private:
    PCF8575 pcf_;
    Config cfg_;

    void driveRow(uint8_t r)
    {
        uint16_t v = 0xFFFF; // all high (inputs)
        // active row driven low
        v &= ~(1u << cfg_.rows[r]);
        // non-active rows high
        for (uint8_t i = 0; i < 3; i++)
        {
            if (i != r)
                v |= (1u << cfg_.rows[i]);
        }
        pcf_.write(v);
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

    void onControl(uint8_t c, bool down)
    {
        Serial1.printf("CTL %u %s\n", c, down ? "DOWN" : "UP");
        if (!down) return; // only on press
        switch (c)
        {
        case 0: // Play/Pause
            if (rl_)
                rl_->post(AppEvent{ tx_ && tx_->isRunning() ? AppEvent::Type::Pause : AppEvent::Type::Play });
            break;
        case 1: // Stop
            if (rl_) rl_->post(AppEvent{ AppEvent::Type::Stop });
            break;
        case 2: // Rec arm toggle
            if (rec_) rec_->arm(!rec_->isArmed());
            break;
        case 5: // root-
            setRoot((root_ + 11) % 12);
            break;
        case 6: // root+
            setRoot((root_ + 1) % 12);
            break;
        case 7: // shift (reserved)
            break;
        default:
            break;
        }
    }

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

        if (btn >= 8)
        {
            uint8_t k = btn - 8;
            return clamp(base + bot_[k]);
        } // q..i naturals

        if (btn == 0)
            return -1; // no black before first

        uint8_t g = btn - 1;

        if (g >= 7)
            return -1; // gap 0..6

        uint8_t diff = bot_[g + 1] - bot_[g];

        if (diff == 2)
            return clamp(base + bot_[g] + 1); // whole-tone gap → black

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
        Serial.printf("Note ON %u (btn %u) ch%u v%u\n", p, btn, ch, vel_);
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
        Serial.printf("Note OFF %u (btn %u) ch%u\n", p, btn, ch);
        midi.send({ch, (uint8_t)p, 0, false, 0});
        if (rec_ && tx_ && rec_->isArmed())
            rec_->onLiveNoteOff((uint8_t)p, tx_->playTick());
        pressed_[btn] = false;
        pitch_[btn] = -1;
    }

    bool lastDown_[3][8] = {};   // last read state per row/col
    uint32_t debUntil_[16] = {}; // per-button debounce time (micros)
    bool pressed_[16] = {};      // per-button pressed state
    int16_t pitch_[16] = {};     // per-button pitch (or -1)

    uint8_t root_{0}, oct_{4}, vel_{100};
    uint8_t bot_[8] = {}; // bottom row natural note offsets (from root)
    // External systems
    RunLoop *rl_{nullptr};
    RecordEngine *rec_{nullptr};
    Transport *tx_{nullptr};
};