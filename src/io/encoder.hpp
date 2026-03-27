#pragma once
#include <Arduino.h>

// Single rotary encoder with integrated push switch.
// Quadrature decoding via gray-code state table. Debounced switch with edge detection.
class Encoder {
public:
    void begin(uint8_t pinA, uint8_t pinB, uint8_t pinSW, uint32_t debounceUs = 5000) {
        pinA_ = pinA; pinB_ = pinB; pinSW_ = pinSW;
        debounceUs_ = debounceUs;
        pinMode(pinA_, INPUT_PULLUP);
        pinMode(pinB_, INPUT_PULLUP);
        pinMode(pinSW_, INPUT_PULLUP);
        lastStateAB_   = readAB();
        lastSwitch_    = digitalRead(pinSW_) == LOW;
        switchPressed_ = switchReleased_ = false;
        debounceUntil_ = 0;
        delta_         = 0;
    }

    // Poll for rotation. Returns accumulated delta since last call (+CW, -CCW, 0=none).
    int8_t poll() {
        uint32_t now = micros();
        uint8_t curAB = readAB();
        if (curAB != lastStateAB_) {
            static const int8_t table[4][4] = {
                { 0,-1,+1, 0},
                {+1, 0, 0,-1},
                {-1, 0, 0,+1},
                { 0,+1,-1, 0}
            };
            delta_ += table[lastStateAB_][curAB];
            lastStateAB_ = curAB;
        }
        int8_t result = delta_;
        delta_ = 0;

        if ((int32_t)(now - debounceUntil_) >= 0) {
            bool sw = digitalRead(pinSW_) == LOW;
            if (sw != lastSwitch_) {
                lastSwitch_ = sw;
                if (sw) switchPressed_  = true;
                else    switchReleased_ = true;
                debounceUntil_ = now + debounceUs_;
            }
        }
        return result;
    }

    bool wasPressed()  { bool r = switchPressed_;  switchPressed_  = false; return r; }
    bool wasReleased() { bool r = switchReleased_; switchReleased_ = false; return r; }
    bool isPressed()   const { return lastSwitch_; }

    void reset() {
        delta_ = 0; switchPressed_ = switchReleased_ = false;
        lastStateAB_ = readAB();
        lastSwitch_  = digitalRead(pinSW_) == LOW;
    }

private:
    uint8_t  pinA_{0}, pinB_{0}, pinSW_{0};
    uint32_t debounceUs_{5000};
    uint32_t debounceUntil_{0};
    uint8_t  lastStateAB_{0};
    int8_t   delta_{0};
    bool     lastSwitch_{false};
    bool     switchPressed_{false};
    bool     switchReleased_{false};

    uint8_t readAB() const {
        return ((digitalRead(pinB_) == HIGH) ? 2 : 0) |
               ((digitalRead(pinA_) == HIGH) ? 1 : 0);
    }
};
