#pragma once
#include <Arduino.h>
#include <array>

#include "base_view.hpp"

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
     * Initialize all registered views.
     * @param midiChannel MIDI channel to pass to all views
     */
    void beginAll(uint8_t midiChannel);

    /**
     * Attach core systems to all registered views, including this ViewManager for view switching.
     * @param runLoop Main run loop
     * @param recordEngine Recording engine
     * @param transport Transport system
     */
    void attachAll(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport);

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
     * Poll input for the current active view.
     */
    void poll(MidiIO& midi);


    /**
     * Get name of current view for display.
     */
    const char* getCurrentViewName() const;

private:
    std::array<IView*, static_cast<size_t>(ViewType::Count)> views_{};
    ViewType currentViewType_{ViewType::Performance};
    IView* currentView_{nullptr};

    void activateView(ViewType viewType);
};