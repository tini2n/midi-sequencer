#pragma once
#include <Wire.h>

class PCF8575
{
public:
    bool begin(uint8_t address = 0x20)
    {
        address_ = address;
        Wire.begin();
        Wire.setClock(400000);
        return ping();
    }

    bool ping()
    {
        Wire.beginTransmission(address_);
        return (Wire.endTransmission() == 0);
    }

    bool write(uint16_t value)
    {
        out_ = value;
        Wire.beginTransmission(address_);
        Wire.write(value & 0xFF);
        Wire.write(value >> 8);
        return Wire.endTransmission() == 0;
    }

    // Read current pin states into `value`. Returns true on success.
    bool read(uint16_t &value)
    {
        Wire.requestFrom(address_, (uint8_t)2);
        if (Wire.available() < 2)
            return false;
        uint8_t lo = Wire.read();
        uint8_t hi = Wire.read();
        value = (uint16_t)lo | ((uint16_t)hi << 8);
        return true;
    }

    uint16_t latch() const { return out_; }

private:
    uint8_t address_{0x20};
    uint16_t out_{0xFFFF};
};
