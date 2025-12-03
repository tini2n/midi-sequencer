#pragma once
#include <stdint.h>

class MidiIO;
class Pattern;

/**
 * Mode configuration parameters.
 * Used by MatrixKB::configureMode() to set mode-specific values without tight coupling.
 */
struct IModeConfig
{
    // Performance mode parameters
    uint8_t root{0};
    int8_t octave{4};
    uint8_t velocity{100};
    
    // Cursor mode parameters
    uint8_t editPitch{60};
};

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
    
    /**
     * Configure mode parameters (type-safe alternative to specific getters/setters).
     * @param config Configuration structure with mode-specific parameters
     */
    virtual void configure(const IModeConfig& config) = 0;
    
    /**
     * Read current mode configuration.
     * @param config Output parameter filled with current mode state
     */
    virtual void getConfig(IModeConfig& config) const = 0;
    
    /**
     * Handle control button events (transport, shift, etc.).
     * @param c Control button index (0-7)
     * @param down Button state (true=pressed, false=released)
     * @param shift Shift key state
     * @param context Mode-specific context (e.g., Pattern* for cursor mode)
     * @return true if event was handled by mode, false to use default handling
     */
    virtual bool onControl(uint8_t c, bool down, bool shift, void* context) {
        (void)c; (void)down; (void)shift; (void)context;
        return false; // Default: event not handled
    }
};
