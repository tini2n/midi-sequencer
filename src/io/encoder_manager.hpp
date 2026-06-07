#pragma once
#include <Arduino.h>
#include "encoder.hpp"

struct EncoderRotationEvent { uint8_t encoderId; int8_t delta; };
struct EncoderButtonEvent   { uint8_t encoderId; bool pressed; };

class IEncoderHandler {
public:
    virtual ~IEncoderHandler() = default;
    virtual void onEncoderRotation(const EncoderRotationEvent&) {}
    virtual void onEncoderButton(const EncoderButtonEvent&)     {}
};

// Manager for up to 8 rotary encoders. Poll from the main loop (not ISR).
class EncoderManager {
public:
    static constexpr uint8_t NUM_ENCODERS = 8;

    // reversed=true inverts the delta in software — use when A and B are
    // physically swapped on the PCB so that L always yields −1 and R yields +1.
    struct PinConfig { uint8_t pinA, pinB, pinSW; bool reversed{false}; };

    void begin(const PinConfig configs[NUM_ENCODERS], uint32_t debounceUs = 5000) {
        for (uint8_t i = 0; i < NUM_ENCODERS; i++)
            encoders_[i].begin(configs[i].pinA, configs[i].pinB, configs[i].pinSW,
                               debounceUs, configs[i].reversed);
    }

    void setHandler(IEncoderHandler* h) { handler_ = h; }

    void poll() {
        if (!handler_) return;
        for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
            int8_t delta = encoders_[i].poll();
            if (delta) handler_->onEncoderRotation({i, delta});
            if (encoders_[i].wasPressed())  handler_->onEncoderButton({i, true});
            if (encoders_[i].wasReleased()) handler_->onEncoderButton({i, false});
        }
    }

    void reset() { for (uint8_t i = 0; i < NUM_ENCODERS; i++) encoders_[i].reset(); }
    Encoder& getEncoder(uint8_t id) { return encoders_[id < NUM_ENCODERS ? id : 0]; }

private:
    Encoder          encoders_[NUM_ENCODERS];
    IEncoderHandler* handler_{nullptr};
};
