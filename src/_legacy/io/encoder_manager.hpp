#pragma once
#include <Arduino.h>
#include "io/encoder.hpp"

/**
 * Event structure for encoder rotation.
 */
struct EncoderRotationEvent
{
    uint8_t encoderId; // 0-7
    int8_t delta;      // Rotation amount: negative=CCW, positive=CW
};

/**
 * Event structure for encoder switch press.
 */
struct EncoderButtonEvent
{
    uint8_t encoderId; // 0-7
    bool pressed;      // true on press, false on release
};

/**
 * Interface for handling encoder events.
 * Views should implement this to respond to encoder input.
 */
class IEncoderHandler
{
public:
    virtual ~IEncoderHandler() = default;
    
    /**
     * Called when an encoder is rotated.
     * @param event Rotation event with encoder ID and delta
     */
    virtual void onEncoderRotation(const EncoderRotationEvent& event) {}
    
    /**
     * Called when an encoder switch is pressed.
     * @param event Button event with encoder ID and state
     */
    virtual void onEncoderButton(const EncoderButtonEvent& event) {}
};

/**
 * Manager for 8 rotary encoders with integrated switches.
 * Handles polling, event generation, and dispatching to handlers.
 * Supports mode-specific behavior via handler interface.
 */
class EncoderManager
{
public:
    static constexpr uint8_t NUM_ENCODERS = 8;
    
    struct PinConfig
    {
        uint8_t pinA;
        uint8_t pinB;
        uint8_t pinSW;
    };
    
    /**
     * Initialize all 8 encoders with pin configuration.
     * @param configs Array of 8 pin configurations
     * @param debounceUs Debounce time in microseconds (default 5000)
     */
    void begin(const PinConfig configs[NUM_ENCODERS], uint32_t debounceUs = 5000)
    {
        for (uint8_t i = 0; i < NUM_ENCODERS; i++)
        {
            encoders_[i].begin(configs[i].pinA, configs[i].pinB, configs[i].pinSW, debounceUs);
        }
    }
    
    /**
     * Set the handler for encoder events.
     * Pass nullptr to disable event handling.
     * @param handler Pointer to handler implementing IEncoderHandler
     */
    void setHandler(IEncoderHandler* handler)
    {
        handler_ = handler;
    }
    
    /**
     * Poll all encoders and dispatch events to handler.
     * Call this frequently (e.g., every 1ms or in main loop).
     */
    void poll()
    {
        if (handler_ == nullptr) return;
        
        for (uint8_t i = 0; i < NUM_ENCODERS; i++)
        {
            // Check rotation
            int8_t delta = encoders_[i].poll();
            if (delta != 0)
            {
                EncoderRotationEvent evt{i, delta};
                handler_->onEncoderRotation(evt);
            }
            
            // Check button press (edge detection)
            if (encoders_[i].wasPressed())
            {
                EncoderButtonEvent evt{i, true};
                handler_->onEncoderButton(evt);
            }
            
            // Check button release (edge detection)
            if (encoders_[i].wasReleased())
            {
                EncoderButtonEvent evt{i, false};
                handler_->onEncoderButton(evt);
            }
        }
    }
    
    /**
     * Reset all encoder states (useful on view change).
     */
    void reset()
    {
        for (uint8_t i = 0; i < NUM_ENCODERS; i++)
        {
            encoders_[i].reset();
        }
    }
    
    /**
     * Get reference to specific encoder (for direct access if needed).
     */
    Encoder& getEncoder(uint8_t id)
    {
        if (id >= NUM_ENCODERS) id = 0;
        return encoders_[id];
    }

private:
    Encoder encoders_[NUM_ENCODERS];
    IEncoderHandler* handler_{nullptr};
};
