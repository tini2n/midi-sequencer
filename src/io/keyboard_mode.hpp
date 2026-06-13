#pragma once
#include <stdint.h>

class MidiIO;
class Pattern;

// Configuration exchanged between keyboard modes (avoids tight coupling).
struct IModeConfig {
    uint8_t root{0};
    int8_t  octave{4};
    uint8_t velocity{100};
    uint8_t editPitch{60};
};

// Strategy interface — defines what pressing a key *means*.
// MatrixKB (hardware driver) delegates all button events to the active IKeyboardMode.
class IKeyboardMode {
public:
    virtual ~IKeyboardMode() = default;

    virtual void onButtonDown(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) = 0;
    virtual void onButtonUp(uint8_t btn, MidiIO& midi, uint8_t ch, void* context) = 0;
    virtual void update(uint32_t now, void* context) {}
    virtual void onActivate() {}
    virtual void onDeactivate() {}
    virtual void configure(const IModeConfig& config) = 0;
    virtual void getConfig(IModeConfig& config) const = 0;

    // Return true if the control event was handled (suppresses MatrixKB default handling).
    virtual bool onControl(uint8_t c, bool down, bool shift, void* context) {
        (void)c; (void)down; (void)shift; (void)context;
        return false;
    }
};
