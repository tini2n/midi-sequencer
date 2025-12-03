#pragma once
#include <Arduino.h>
#include "matrix_kb_mode.hpp"
#include "core/midi_io.hpp"
#include "core/transport.hpp"
#include "core/timebase.hpp"
#include "engine/record_engine.hpp"
#include "model/scale.hpp"

/**
 * Performance keyboard mode for matrix keyboard.
 * Maps 16 buttons to piano-style note triggering based on root, scale, and octave.
 * Note: Named PerformanceKBMode to avoid conflict with PerformanceMode enum in performance_state.hpp
 * 
 * Button Layout (2×8):
 *   Top:    [0][1][2][3][4][5][6][7]     → Black keys / gaps
 *   Bottom: [8][9][10][11][12][13][14][15] → White keys / naturals
 */
class PerformanceKBMode : public IMatrixKBMode
{
public:
    void onButtonDown(uint8_t btn, MidiIO &midi, uint8_t ch, void *context) override;
    void onButtonUp(uint8_t btn, MidiIO &midi, uint8_t ch, void *context) override;
    void onActivate() override;
    void onDeactivate() override;

    // Performance mode specific API
    void setRoot(uint8_t r);
    void setOctave(int8_t o);
    void setVelocity(uint8_t v) { vel_ = v; }
    void setScale(Scale s);
    void setFold(bool f) { fold_ = f; }
    void reset();

    // External systems for recording hooks
    void attach(RecordEngine *rec, Transport *tx)
    {
        rec_ = rec;
        tx_ = tx;
    }

    uint8_t getRoot() const { return root_; }
    int8_t getOctave() const { return oct_; }
    uint8_t getVelocity() const { return vel_; }
    
    // IMatrixKBMode configuration interface
    void configure(const IModeConfig& config) override
    {
        root_ = config.root;
        oct_ = config.octave;
        vel_ = config.velocity;
    }
    
    void getConfig(IModeConfig& config) const override
    {
        config.root = root_;
        config.octave = oct_;
        config.velocity = vel_;
    }

private:
    bool pressed_[16] = {};  // per-button pressed state
    int16_t pitch_[16] = {}; // per-button pitch (or -1)

    uint8_t root_{0}, oct_{4}, vel_{100};
    Scale scale_{Scale::None};
    bool fold_{false};
    uint8_t bot_[8] = {}; // bottom row natural note offsets (from root)

    // External systems
    RecordEngine *rec_{nullptr};
    Transport *tx_{nullptr};

    // Helpers
    void computeBottomOffsets();
    int btnToPitch(uint8_t btn) const;
    static int clamp(int p) { return p < 0 ? 0 : (p > 127 ? 127 : p); }
    void noteOn(int btn, MidiIO &midi, uint8_t ch, int *lastPitchOpt);
    void noteOff(int btn, MidiIO &midi, uint8_t ch);
};
