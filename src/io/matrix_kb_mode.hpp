#pragma once
#include <stdint.h>

class MidiIO;
class Pattern;

/**
 * Strategy interface for matrix keyboard behavior modes.
 * Decouples hardware scanning from button interpretation logic.
 */
class IMatrixKBMode
{
public:
    virtual ~IMatrixKBMode() = default;

    /**
     * Called when a button is pressed (after debouncing).
     * @param btn Button index (0-15 for musical/step keys)
     * @param midi MIDI output interface
     * @param ch MIDI channel (1-16)
     * @param context Mode-specific context (e.g., Pattern* for cursor mode)
     */
    virtual void onButtonDown(uint8_t btn, MidiIO &midi, uint8_t ch, void *context) = 0;

    /**
     * Called when a button is released (after debouncing).
     */
    virtual void onButtonUp(uint8_t btn, MidiIO &midi, uint8_t ch, void *context) = 0;

    /**
     * Optional: called periodically for mode-specific updates.
     * Used for gesture detection, timing, etc.
     */
    virtual void update(uint32_t now, void *context) {}

    /**
     * Called when mode is activated (view switch).
     */
    virtual void onActivate() {}

    /**
     * Called when mode is deactivated.
     */
    virtual void onDeactivate() {}
};
