#pragma once
#include <Arduino.h>

/**
 * Single rotary encoder with integrated push switch.
 * Handles quadrature decoding, debouncing, and switch state.
 * Uses gray-code state machine for reliable rotation detection.
 */
class Encoder
{
public:
    /**
     * Initialize encoder with pin assignments.
     * @param pinA Encoder phase A pin
     * @param pinB Encoder phase B pin
     * @param pinSW Switch pin (active LOW with internal pullup)
     * @param debounceUs Debounce time in microseconds (default 5000)
     */
    void begin(uint8_t pinA, uint8_t pinB, uint8_t pinSW, uint32_t debounceUs = 5000)
    {
        pinA_ = pinA;
        pinB_ = pinB;
        pinSW_ = pinSW;
        debounceUs_ = debounceUs;
        
        // Configure encoder pins with pullups
        pinMode(pinA_, INPUT_PULLUP);
        pinMode(pinB_, INPUT_PULLUP);
        pinMode(pinSW_, INPUT_PULLUP);
        
        // Initialize state
        lastStateAB_ = readAB();
        lastSwitch_ = digitalRead(pinSW_) == LOW;
        switchPressed_ = false;
        debounceUntil_ = 0;
        
        // Reset delta
        delta_ = 0;
    }
    
    /**
     * Poll encoder for changes. Call frequently (e.g., every 1ms or in main loop).
     * Returns rotation delta since last read (-N for CCW, +N for CW, 0 for no change).
     */
    int8_t poll()
    {
        uint32_t now = micros();
        
        // Quadrature decoding using gray-code state machine
        uint8_t curAB = readAB();
        if (curAB != lastStateAB_)
        {
            // State transition lookup table for quadrature decoding
            // [lastState][currentState] -> direction
            // CW sequence:  00 -> 01 -> 11 -> 10 -> 00
            // CCW sequence: 00 -> 10 -> 11 -> 01 -> 00
            static const int8_t table[4][4] = {
                //  00  01  10  11  <- current
                {   0, -1, +1,  0 }, // 00 <- last
                {  +1,  0,  0, -1 }, // 01
                {  -1,  0,  0, +1 }, // 10
                {   0, +1, -1,  0 }  // 11
            };
            
            int8_t dir = table[lastStateAB_][curAB];
            delta_ += dir;
            lastStateAB_ = curAB;
        }
        
        // Read current rotation delta and reset
        int8_t result = delta_;
        delta_ = 0;
        
        // Switch debouncing
        if (now >= debounceUntil_)
        {
            bool sw = digitalRead(pinSW_) == LOW; // Active LOW
            if (sw != lastSwitch_)
            {
                lastSwitch_ = sw;
                switchPressed_ = sw; // True on press edge
                debounceUntil_ = now + debounceUs_;
            }
        }
        
        return result;
    }
    
    /**
     * Check if switch was pressed since last poll.
     * Returns true only once per press (edge detection).
     */
    bool wasPressed()
    {
        bool result = switchPressed_;
        switchPressed_ = false;
        return result;
    }
    
    /**
     * Check current switch state (level detection).
     * @return true if switch is currently held down
     */
    bool isPressed() const
    {
        return lastSwitch_;
    }
    
    /**
     * Reset encoder state (useful on view change, etc.)
     */
    void reset()
    {
        delta_ = 0;
        switchPressed_ = false;
        lastStateAB_ = readAB();
        lastSwitch_ = digitalRead(pinSW_) == LOW;
    }

private:
    uint8_t pinA_{0}, pinB_{0}, pinSW_{0};
    uint32_t debounceUs_{5000};
    uint32_t debounceUntil_{0};
    
    // Encoder state
    uint8_t lastStateAB_{0};  // Last AB state (0-3)
    int8_t delta_{0};         // Accumulated rotation since last read
    
    // Switch state
    bool lastSwitch_{false};     // Last debounced switch state
    bool switchPressed_{false};  // Edge flag: true on press
    
    /**
     * Read current AB state as 2-bit value (B=bit1, A=bit0).
     */
    uint8_t readAB() const
    {
        uint8_t a = digitalRead(pinA_) == HIGH ? 1 : 0;
        uint8_t b = digitalRead(pinB_) == HIGH ? 1 : 0;
        return (b << 1) | a;
    }
};
