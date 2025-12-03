#pragma once
#include <Wire.h>

/**
 * PCF8575 I2C I/O Expander Driver
 * Optimized for matrix keyboard scanning with minimal overhead.
 */
class PCF8575
{
public:
    bool begin(uint8_t address = 0x20)
    {
        address_ = address;
        // Note: Wire.begin() and Wire.setClock() must be called in main.cpp
        // before calling this method
        
        out_ = 0xFFFF;
        
        // Verify device is present
        Wire.beginTransmission(address_);
        uint8_t result = Wire.endTransmission();
        if (result != 0) {
            Serial.printf("PCF8575 not found at 0x%02X (error=%u)\n", address_, result);
            return false;
        }
        
        // Initialize all pins as inputs (high)
        if (!write(0xFFFF)) {
            Serial.printf("PCF8575 initial write failed at 0x%02X\n", address_);
            return false;
        }
        Serial.printf("PCF8575 OK at 0x%02X\n", address_);
        return true;
    }

    /**
     * Write to PCF8575. Pins set to 1 are inputs (weak pull-up), 0 are outputs (drive low).
     */
    bool write(uint16_t value)
    {
        Wire.beginTransmission(address_);
        Wire.write((uint8_t)(value & 0xFF));
        Wire.write((uint8_t)(value >> 8));
        
        if (Wire.endTransmission() == 0) {
            out_ = value;
            return true;
        }
        return false;
    }

    /**
     * Read current pin states. Returns true on success.
     */
    bool read(uint16_t &value)
    {
        Wire.requestFrom(address_, (uint8_t)2);
        
        uint32_t start = micros();
        while (Wire.available() < 2) {
            if ((int32_t)(micros() - start) > 1000) { // 1ms timeout
            // Flush any stray bytes to avoid corrupting next read
            while (Wire.available() > 0) { (void)Wire.read(); }
                return false;
            }
        }
        
        uint8_t lo = Wire.read();
        uint8_t hi = Wire.read();
        value = (uint16_t)lo | ((uint16_t)hi << 8);
        // Drain any unexpected extra bytes defensively
        while (Wire.available() > 0) { (void)Wire.read(); }
        return true;
    }

    uint16_t latch() const { return out_; }

private:
    uint8_t address_{0x20};
    uint16_t out_{0xFFFF};
};
