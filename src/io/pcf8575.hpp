#pragma once
#include <Arduino.h>
#include <Wire.h>

// PCF8575 I2C 16-bit I/O expander driver.
// Pins driven LOW = output (active). Pins set HIGH = input (weak pull-up).
class PCF8575
{
public:
    bool begin(uint8_t address = 0x20, uint32_t wireClock = 400000)
    {
        address_ = address;
        ok_ = false;
        Wire.begin();
        Wire.setClock(wireClock);
        out_ = 0xFFFF;
        Wire.beginTransmission(address_);
        if (Wire.endTransmission() != 0) {
            Serial.printf("[PCF8575] not found at 0x%02X\n", address_);
            return false;
        }
        if (!writeRaw(0xFFFF)) {
            Serial.printf("[PCF8575] found but write failed at 0x%02X\n", address_);
            return false;
        }
        Serial.printf("[PCF8575] OK at 0x%02X\n", address_);
        ok_ = true;
        return true;
    }

    bool ok() const { return ok_; }

    // write/read return false immediately when not initialised — no blocking.
    bool write(uint16_t value)
    {
        if (!ok_) return false;
        return writeRaw(value);
    }

    bool read(uint16_t& value)
    {
        if (!ok_) return false;
        Wire.requestFrom(address_, (uint8_t)2);
        uint32_t start = micros();
        while (Wire.available() < 2) {
            if ((int32_t)(micros() - start) > 1000) {
                while (Wire.available()) (void)Wire.read();
                return false;
            }
        }
        uint8_t lo = Wire.read();
        uint8_t hi = Wire.read();
        value = (uint16_t)lo | ((uint16_t)hi << 8);
        while (Wire.available()) (void)Wire.read();
        return true;
    }

    uint16_t latch() const { return out_; }

private:
    bool writeRaw(uint16_t value)
    {
        Wire.beginTransmission(address_);
        Wire.write((uint8_t)(value & 0xFF));
        Wire.write((uint8_t)(value >> 8));
        if (Wire.endTransmission() == 0) { out_ = value; return true; }
        return false;
    }

    uint8_t  address_{0x20};
    uint16_t out_{0xFFFF};
    bool     ok_{false};
};
