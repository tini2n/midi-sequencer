#pragma once
#include <Arduino.h>
#include <Wire.h>

// PCF8575 I2C 16-bit I/O expander driver.
// Pins driven LOW = output (active). Pins set HIGH = input (weak pull-up).
// Used for the matrix keyboard row/column scanning.
class PCF8575
{
public:
    // 9-clock bus recovery — only needed when SDA is physically stuck LOW.
    // Do NOT call when SDA is HIGH; spurious clock pulses confuse a healthy PCF.
    static void busRecover(uint8_t sdaPin = 18, uint8_t sclPin = 19) {
        pinMode(sdaPin, INPUT_PULLUP);
        pinMode(sclPin, OUTPUT);
        delayMicroseconds(10);
        for (int i = 0; i < 9; i++) {
            digitalWrite(sclPin, LOW);  delayMicroseconds(5);
            digitalWrite(sclPin, HIGH); delayMicroseconds(5);
            if (digitalRead(sdaPin)) break;
        }
        pinMode(sdaPin, OUTPUT);
        digitalWrite(sdaPin, LOW);  delayMicroseconds(5);
        digitalWrite(sclPin, HIGH); delayMicroseconds(5);
        digitalWrite(sdaPin, HIGH); delayMicroseconds(5);
        pinMode(sdaPin, INPUT_PULLUP);
        pinMode(sclPin, INPUT_PULLUP);
        delayMicroseconds(100);
    }

    bool begin(uint8_t address = 0x20, uint32_t wireClock = 400000)
    {
        address_ = address;
        ok_ = false;

        // Only recover if SDA is stuck LOW.
        pinMode(18, INPUT_PULLUP);
        delayMicroseconds(200);
        if (!digitalRead(18)) {
            Serial.println("[PCF8575] SDA stuck LOW — running bus recovery");
            busRecover();
        }

        Wire.begin();
        Wire.setClock(wireClock);
        out_ = 0xFFFF;

        // Retry up to 3 times — PCF power-on can race with Teensy boot.
        for (int attempt = 0; attempt < 3; attempt++) {
            if (attempt > 0) delay(100);
            Wire.beginTransmission(address_);
            if (Wire.endTransmission() != 0) continue;
            if (!writeRaw(0xFFFF)) continue;
            Serial.printf("[PCF8575] OK at 0x%02X (attempt %d)\n", address_, attempt + 1);
            ok_ = true;
            return true;
        }

        Serial.printf("[PCF8575] not found at 0x%02X after 3 attempts\n", address_);
        return false;
    }

    bool ok() const { return ok_; }

    // write/read return false immediately when not initialised — no blocking.
    bool write(uint16_t value)
    {
        if (!ok_) return false;
        return writeRaw(value);
    }

    bool read(uint16_t &value)
    {
        if (!ok_) return false;
        Wire.requestFrom(address_, (uint8_t)2);
        uint32_t start = micros();
        while (Wire.available() < 2)
        {
            if ((int32_t)(micros() - start) > 1000)
            {
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
