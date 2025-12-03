#pragma once
#include <Arduino.h>
#include <array>

#include "base_view.hpp"
#include "io/encoder_manager.hpp"

/**
 * Manages multiple views and handles switching between them.
 * Provides a unified interface for the main loop to interact with views.
 */
class ViewManager
{
public:
    ViewManager() = default;
    ~ViewManager() = default;

    /**
     * Register a view with the manager.
     * @param viewType Type of view to register
     * @param view Pointer to view instance (manager does not take ownership)
     */
    void registerView(ViewType viewType, IView* view);

    /**
     * Initialize all views and encoders (call once after registering all views).
     * @param runLoop Main run loop
     * @param recordEngine Recording engine
     * @param transport Transport system
     * @param pattern Pattern reference
     * @param midiChannel MIDI channel
     * @param encoderConfigs Encoder pin configurations
     * @param encoderDebounceUs Encoder debounce time in microseconds
     */
    void initialize(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport,
                   Pattern* pattern, uint8_t midiChannel,
                   const EncoderManager::PinConfig encoderConfigs[EncoderManager::NUM_ENCODERS],
                   uint32_t encoderDebounceUs = 5000);

    /**
     * Switch to a different view.
     * @param viewType Type of view to switch to
     * @return true if switch was successful, false if view not registered
     */
    bool switchToView(ViewType viewType);

    /**
     * Get current active view type.
     */
    ViewType getCurrentViewType() const { return currentViewType_; }

    /**
     * Get current active view instance.
     */
    IView* getCurrentView() const { return currentView_; }

    /**
     * Switch to next view in sequence (for easy cycling).
     */
    void switchToNextView();

    /**
     * Switch to previous view in sequence.
     */
    void switchToPreviousView();

    /**
     * Render the current active view.
     */
    void draw(Pattern& pattern, Viewport& viewport, OledRenderer& oled, 
              MidiIO& midi, uint32_t now, uint32_t playTick);

    /**
     * Poll keyboard input for the current active view.
     */
    void pollKB(MidiIO& midi);

    /**
     * Poll encoders and dispatch to current view.
     */
    void pollEncoders();

    /**
     * Get name of current view for display.
     */
    const char* getCurrentViewName() const;

private:
    std::array<IView*, static_cast<size_t>(ViewType::Count)> views_{};
    ViewType currentViewType_{ViewType::Performance};
    IView* currentView_{nullptr};
    EncoderManager encoderMgr_{};
    bool encodersInitialized_{false};
};