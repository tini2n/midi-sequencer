#pragma once
#include <Arduino.h>

namespace cfg {
    constexpr uint32_t TICK_HZ = 1000; // Tick frequency in Hz
    constexpr uint32_t TICK_US = 1000000UL / TICK_HZ; // Maximum microseconds per tick
    constexpr size_t RB_CAP = 1024; // Ring buffer capacity
    constexpr uint8_t PCF_ADDRESS = 0x20; // I2C address for PCF8575
}