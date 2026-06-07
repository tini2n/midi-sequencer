#pragma once
#include <Arduino.h>

// Single rotary encoder with integrated push switch.
//
// Rotation: emits exactly ±1 per mechanical detent. EC11-type encoders with
// INPUT_PULLUP + common to GND rest at readAB()=0b11 (both channels HIGH).
// Quadrature transitions accumulate in raw_; a value is emitted only when the
// shaft snaps back to the rest state (kDetent), so pre-click mechanical noise
// and partial rotations are silently discarded.
//
// Direction: if A and B are physically swapped on the PCB, set reversed=true
// in begin(). The delta is negated in software so L always produces −1 and R
// always produces +1 regardless of wiring polarity.
//
// Sensitivity note: kTable lives as a function-local static constexpr inside
// poll() rather than a class-level static member. This avoids a subtle ARM GCC
// issue where a class-level static constexpr array can be treated as ODR-used
// and read as zeros if no out-of-class definition is present. If sensitivity
// is still poor after verifying the rest state, consider interrupt-driven
// counting (attachInterrupt on pinA_/pinB_).
class Encoder {
public:
    void begin(uint8_t pinA, uint8_t pinB, uint8_t pinSW,
               uint32_t debounceUs = 5000, bool reversed = false) {
        pinA_ = pinA; pinB_ = pinB; pinSW_ = pinSW;
        debounceUs_ = debounceUs;
        reversed_   = reversed;
        pinMode(pinA_, INPUT_PULLUP);
        pinMode(pinB_, INPUT_PULLUP);
        pinMode(pinSW_, INPUT_PULLUP);
        lastStateAB_   = readAB();
        lastSwitch_    = digitalRead(pinSW_) == LOW;
        switchPressed_ = switchReleased_ = false;
        debounceUntil_ = 0;
        raw_           = 0;
    }

    // Returns ±1 per detent click, 0 otherwise.
    // Sign is already corrected for reversal; caller sees L=−1, R=+1 always.
    int8_t poll() {
        // Function-local static: guaranteed to live in .rodata with no ODR risk.
        static constexpr int8_t kTable[4][4] = {
            { 0,-1,+1, 0},
            {+1, 0, 0,-1},
            {-1, 0, 0,+1},
            { 0,+1,-1, 0}
        };
        // EC11 at rest: both A and B open → pulled HIGH → readAB() = 0b11.
        static constexpr uint8_t kDetent = 0b11;

        uint32_t now   = micros();
        uint8_t  curAB = readAB();

        if (curAB != lastStateAB_) {
            raw_ += kTable[lastStateAB_][curAB];
            lastStateAB_ = curAB;
        }

        int8_t result = 0;
        if (curAB == kDetent && raw_ != 0) {
            result = (raw_ > 0) ? 1 : -1;
            raw_   = 0;
        }
        if (reversed_ && result != 0) result = -result;

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
        raw_ = 0; switchPressed_ = switchReleased_ = false;
        lastStateAB_ = readAB();
        lastSwitch_  = digitalRead(pinSW_) == LOW;
    }

private:
    uint8_t  pinA_{0}, pinB_{0}, pinSW_{0};
    uint32_t debounceUs_{5000};
    uint32_t debounceUntil_{0};
    uint8_t  lastStateAB_{0};
    int8_t   raw_{0};
    bool     reversed_{false};
    bool     lastSwitch_{false};
    bool     switchPressed_{false};
    bool     switchReleased_{false};

    uint8_t readAB() const {
        return ((digitalRead(pinB_) == HIGH) ? 2 : 0) |
               ((digitalRead(pinA_) == HIGH) ? 1 : 0);
    }
};
