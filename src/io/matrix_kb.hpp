#pragma once
#include <Arduino.h>
#include "../core/midi_io.hpp"
#include "../core/runloop.hpp"
#include "../core/transport.hpp"
#include "pcf8575.hpp"
#include "matrix_kb_mode.hpp"
#include "../model/scale.hpp"

// PCF8575-based 3×8 matrix keyboard driver.
// Scans three rows of 8 buttons each:
//   rowTop (0–7)  — top musical/step keys
//   rowBot (8–15) — bottom musical/step keys
//   rowCtl (16+)  — control buttons (transport, shift, etc.)
//
// In step-sequencer (CursorMode), all 16 step buttons are passed to the active mode.
// In piano mode (no mode set), buttons are mapped to MIDI pitches via scale/fold logic.
class MatrixKB {
public:
    struct Config {
        uint8_t address{0x20};
        uint8_t rows[3]{10, 11, 12};
        uint8_t cols[8]{0, 1, 2, 3, 4, 5, 6, 7};
        uint8_t rowTop{1}, rowBot{2}, rowCtl{0};
        uint32_t debounce_us{5000};
    };

    bool begin(const Config& c, uint8_t root = 0, int8_t octave = 4, uint8_t vel = 100) {
        cfg_ = c;
        bool ok = pcf_.begin(cfg_.address);
        if (!ok) Serial.println("[MatrixKB] PCF8575 init failed — keyboard disabled");
        setRoot(root);
        setOctave(octave);
        vel_ = vel;
        reset();
        return ok;
    }

    void setRoot(uint8_t r)   { root_ = r % 12; computeBottomOffsets(); }
    void setOctave(int8_t o)  { oct_  = (o < 0) ? 0 : (o > 10 ? 10 : o); }
    void setVelocity(uint8_t v) { vel_ = v; }
    void setScale(Scale s)    { scale_ = s; }
    void setFold(bool f)      { fold_ = f; }
    void setMode(IMatrixKBMode* m) { mode_ = m; }
    void setModeContext(void* ctx) { modeCtx_ = ctx; }
    // TODO: Phase 5 — attach ViewManager for view switching
    void attachViewManager(class ViewManager*) {}

    void attach(RunLoop* rl, Transport* tx) { rl_ = rl; tx_ = tx; }

    void reset() {
        for (int i = 0; i < 16; i++) { pressed_[i] = false; pitch_[i] = -1; debUntil_[i] = 0; }
        for (int i = 0; i < 8;  i++) { lastDown_[0][i] = lastDown_[1][i] = lastDown_[2][i] = false; }
    }

    void poll(MidiIO& midi, uint8_t ch);

private:
    PCF8575       pcf_;
    Config        cfg_;
    RunLoop*      rl_{nullptr};
    Transport*    tx_{nullptr};
    IMatrixKBMode* mode_{nullptr};
    void*         modeCtx_{nullptr};

    bool     lastDown_[3][8]{};
    uint32_t debUntil_[16]{};
    uint32_t debCtlUntil_[8]{};
    bool     pressed_[16]{};
    int16_t  pitch_[16]{};
    uint16_t lastRaw_{0xFFFF}; // for raw diagnostic in poll()

    uint8_t root_{0}, oct_{4}, vel_{100};
    Scale   scale_{Scale::None};
    bool    fold_{false};
    uint8_t bot_[8]{};

    bool driveRow(uint8_t r);
    int  rowToBtn(uint8_t r, uint8_t c) const;
    void onControl(uint8_t c, bool down);
    int  btnToPitch(uint8_t btn) const;
    void noteOn(int btn, MidiIO& midi, uint8_t ch);
    void noteOff(int btn, MidiIO& midi, uint8_t ch);
    void computeBottomOffsets();
    static int clamp(int p) { return p < 0 ? 0 : (p > 127 ? 127 : p); }
};
