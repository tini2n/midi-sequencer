#pragma once
#include <Arduino.h>

#include "model/pattern.hpp"
#include "model/viewport.hpp"
#include "ui/renderer_oled.hpp"
#include "core/midi_io.hpp"
#include "core/runloop.hpp"
#include "engine/record_engine.hpp"
#include "core/transport.hpp"

/**
 * Abstract base class for all UI views in the sequencer.
 * Provides standardized interface for rendering, input handling, and lifecycle management.
 */
class IView
{
public:
    virtual ~IView() = default;

    /**
     * Initialize the view with given MIDI channel and any required setup.
     */
    virtual void begin(uint8_t midiChannel) = 0;

    /**
     * Attach core system references for transport control, recording, etc.
     * @param runLoop Main run loop for event posting
     * @param recordEngine Recording engine for live recording
     * @param transport Transport for playback control
     * @param viewManager View manager for view switching (optional, can be nullptr)
     */
    virtual void attach(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport, class ViewManager* viewManager = nullptr) = 0;

    /**
     * Render the view to the OLED display.
     * @param pattern Current pattern being played/edited
     * @param viewport Current view into the pattern (time/pitch window)
     * @param oled Renderer for drawing to OLED
     * @param midi MIDI I/O interface
     * @param now Current time in microseconds
     * @param playTick Current playback position in ticks
     */
    virtual void draw(Pattern& pattern, Viewport& viewport, OledRenderer& oled, 
                     MidiIO& midi, uint32_t now, uint32_t playTick) = 0;

    /**
     * Poll for input events and handle them.
     * @param midi MIDI I/O interface for sending immediate events
     */
    virtual void poll(MidiIO& midi) = 0;

    /**
     * Handle view activation (when switching to this view).
     * Called when this view becomes the active view.
     */
    virtual void onActivate() {}

    /**
     * Handle view deactivation (when switching away from this view).
     * Called when another view becomes active.
     */
    virtual void onDeactivate() {}

    /**
     * Get human-readable name for this view (for UI display).
     */
    virtual const char* getName() const = 0;

    /**
     * Get encoder handler interface for this view.
     * @return Pointer to IEncoderHandler implementation, or nullptr if view doesn't handle encoders
     */
    virtual class IEncoderHandler* getEncoderHandler() { return nullptr; }
};

/**
 * Enumeration of available view types for easy switching.
 */
enum class ViewType : uint8_t
{
    Performance = 0,
    Generative = 1,
    Settings = 2,
    // Future views can be added here
    Count
};