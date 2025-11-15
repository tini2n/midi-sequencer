/**
 * Encoder Test Program
 * 
 * Tests all 8 rotary encoders and their integrated switches.
 * Outputs rotation direction and button press events via serial monitor.
 * 
 * Hardware Setup:
 * - 8 rotary encoders with integrated push switches
 * - Connected directly to Teensy 4.1 pins
 * 
 * Pin Mapping:
 * ENC1: A=2,  B=3,  SW=0
 * ENC2: A=4,  B=5,  SW=12
 * ENC3: A=6,  B=7,  SW=26
 * ENC4: A=14, B=15, SW=27
 * ENC5: A=16, B=17, SW=28
 * ENC6: A=20, B=21, SW=29
 * ENC7: A=22, B=23, SW=30
 * ENC8: A=24, B=25, SW=31
 * 
 * Expected Output:
 * - "ENC1 CW: +1" when encoder 1 rotated clockwise
 * - "ENC2 CCW: -1" when encoder 2 rotated counter-clockwise
 * - "ENC3 SW PRESSED" when encoder 3 switch pressed
 * - Counter value for each encoder
 * 
 * Build & Upload:
 * 1. Open this file in PlatformIO
 * 2. Rename to main.cpp (or set as build source)
 * 3. Build and upload to Teensy 4.1
 * 4. Open serial monitor at 115200 baud
 * 5. Rotate encoders and press switches to test
 */

#include <Arduino.h>
#include "io/encoder_manager.hpp"

// Global encoder manager
EncoderManager encoderMgr;

// Counter values for each encoder (for display)
int32_t counters[8] = {0};

// Pin configuration matching hardware setup
const EncoderManager::PinConfig ENCODER_PINS[8] = {
    {2,  3,  0},   // ENC1
    {4,  5,  12},  // ENC2
    {6,  7,  26},  // ENC3
    {14, 15, 27},  // ENC4
    {16, 17, 28},  // ENC5
    {20, 21, 29},  // ENC6
    {22, 23, 30},  // ENC7
    {24, 25, 31}   // ENC8
};

// Handler class for test output
class TestEncoderHandler : public IEncoderHandler
{
public:
    void onEncoderRotation(const EncoderRotationEvent& event) override
    {
        // Update counter
        counters[event.encoderId] += event.delta;
        
        // Print rotation event
        const char* direction = (event.delta > 0) ? "CW" : "CCW";
        Serial.printf("ENC%d %s: %+d  [Counter: %ld]\n", 
                     event.encoderId + 1, 
                     direction, 
                     event.delta,
                     counters[event.encoderId]);
    }
    
    void onEncoderButton(const EncoderButtonEvent& event) override
    {
        // Print button event
        const char* state = event.pressed ? "PRESSED" : "RELEASED";
        Serial.printf("ENC%d SW %s\n", event.encoderId + 1, state);
        
        // Optional: Reset counter on button press
        if (event.pressed)
        {
            counters[event.encoderId] = 0;
            Serial.printf("  -> Counter reset to 0\n");
        }
    }
};

TestEncoderHandler handler;

void setup()
{
    // Initialize serial for debug output
    Serial.begin(115200);
    delay(1000); // Wait for serial to stabilize
    
    Serial.println("\n\n========================================");
    Serial.println("  Encoder Test Program");
    Serial.println("========================================");
    Serial.println("Hardware: 8 Rotary Encoders + Switches");
    Serial.println("Platform: Teensy 4.1");
    Serial.println();
    Serial.println("Instructions:");
    Serial.println("1. Rotate each encoder clockwise/counter-clockwise");
    Serial.println("2. Press each encoder switch");
    Serial.println("3. Observe output below");
    Serial.println();
    Serial.println("Pin Mapping:");
    for (int i = 0; i < 8; i++)
    {
        Serial.printf("  ENC%d: A=%d, B=%d, SW=%d\n", 
                     i + 1,
                     ENCODER_PINS[i].pinA,
                     ENCODER_PINS[i].pinB,
                     ENCODER_PINS[i].pinSW);
    }
    Serial.println("========================================");
    Serial.println();
    
    // Initialize encoder manager with pin configuration
    encoderMgr.begin(ENCODER_PINS, 5000); // 5ms debounce
    encoderMgr.setHandler(&handler);
    
    Serial.println("Encoder manager initialized");
    Serial.println("Ready! Rotate encoders or press switches...");
    Serial.println();
}

void loop()
{
    // Poll encoders continuously
    encoderMgr.poll();
    
    // Small delay to avoid overwhelming serial output
    // (encoder polling is still very responsive)
    delay(1);
}
