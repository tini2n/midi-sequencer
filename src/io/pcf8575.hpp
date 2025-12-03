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
        Wire.begin();
        Wire.setClock(400000); // 400kHz I2C
        
        out_ = 0xFFFF;
        
        // Verify device is present
        Wire.beginTransmission(address_);
        if (Wire.endTransmission() != 0) {
            Serial.printf("PCF8575 not found at 0x%02X\n", address_);
            return false;
        }
        
        // Initialize all pins as inputs (high)
        write(0xFFFF);
        Serial.printf("PCF8575 OK at 0x%02X\n", address_);
        return true;
    }

    /**
     * Write to PCF8575. Pins set to 1 are inputs (weak pull-up), 0 are outputs (drive low).
     */
    bool write(uint16_t value)
    {
        Wire.beginTransmission(address_);
        Wire.write(value & 0xFF);
        Wire.write(value >> 8);
        uint8_t result = Wire.endTransmission();
        
        if (result == 0) {
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
