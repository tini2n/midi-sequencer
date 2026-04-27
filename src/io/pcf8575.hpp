#pragma once
#include <Arduino.h>
#include <Wire.h>

// PCF8575 I2C 16-bit I/O expander driver.
// Pins driven LOW = output (active). Pins set HIGH = input (weak pull-up).
// Used for the matrix keyboard row/column scanning.
class PCF8575
{
public:
    bool begin(uint8_t address = 0x20, uint32_t wireClock = 400000)
    {
        address_ = address;
        Wire.begin();
        Wire.setClock(wireClock);
        out_ = 0xFFFF;

        Wire.beginTransmission(address_);
        if (Wire.endTransmission() != 0)
        {
            Serial.printf("[PCF8575] not found at 0x%02X — check I2C wiring\n", address_);
            return false;
        }
        if (!write(0xFFFF))
        {
            Serial.printf("[PCF8575] write failed at 0x%02X\n", address_);
            return false;
        }
        Serial.printf("[PCF8575] OK at 0x%02X\n", address_);
        return true;
    }

    // Write 16-bit value. Bits=0 drive low (output); bits=1 are inputs (pull-up).
    bool write(uint16_t value)
    {
        Wire.beginTransmission(address_);
        Wire.write((uint8_t)(value & 0xFF));
        Wire.write((uint8_t)(value >> 8));
        if (Wire.endTransmission() == 0)
        {
            out_ = value;
            return true;
        }
        return false;
    }

    // Read current pin states. Returns true on success.
    bool read(uint16_t &value)
    {
        Wire.requestFrom(address_, (uint8_t)2);
        uint32_t start = micros();
        while (Wire.available() < 2)
        {
            if ((int32_t)(micros() - start) > 1000)
            {
                while (Wire.available())
                    (void)Wire.read();
                return false;
            }
        }
        uint8_t lo = Wire.read();
        uint8_t hi = Wire.read();
        value = (uint16_t)lo | ((uint16_t)hi << 8);
        while (Wire.available())
            (void)Wire.read(); // drain extras
        return true;
    }

    uint16_t latch() const { return out_; }

private:
    uint8_t address_{0x20};
    uint16_t out_{0xFFFF};
};
