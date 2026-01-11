#pragma once
#include <Arduino.h>

struct MidiEvent
{
    uint8_t ch;
    uint8_t pitch;
    uint8_t vel;
    bool on;
    uint32_t delay_us;
};

class MidiIO
{
public:
    void begin()
    {
        Serial1.begin(31250);
    }
    void send(const MidiEvent &e)
    {
        if (e.delay_us)
            enqueue(e);
        else
            emit(e);
    }
    void sendClock() { Serial1.write(0xF8); }
    void sendStart() { Serial1.write(0xFA); }
    void sendContinue() { Serial1.write(0xFB); }
    void sendStop() { Serial1.write(0xFC); }
    // Send Note Off on all notes for a channel immediately
    void allNotesOff(uint8_t ch)
    {
        for (uint8_t n = 0; n < 128; ++n)
            emit(MidiEvent{ch, n, 0, false, 0});
    }
    // Send All Notes Off CC (123) and optionally Sound Off CC (120)
    void sendAllNotesOffCC(uint8_t ch, bool soundOff = true)
    {
        uint8_t st = 0xB0 | ((ch - 1) & 0x0F);
        Serial1.write(st); Serial1.write(123); Serial1.write(0);
        if (soundOff) { Serial1.write(st); Serial1.write(120); Serial1.write(0); }
    }
    // Debug helper: send note on/off immediately
    void sendNoteNow(uint8_t ch, uint8_t note, uint8_t vel, bool on)
    {
        emit(MidiEvent{ch, note, vel, on, 0});
        bool debug_{true};
        if (debug_)
            Serial.printf("MIDI %s ch%u n%u v%u\n", on ? "ON" : "OFF", ch, note, vel);
    }
    void update()
    {
        uint32_t now = micros();
        for (size_t i = 0; i < qN_;)
        {
            if ((int32_t)(now - q_[i].due) <= 0)
            {
                break;
            }
            emit(q_[i].m);
            pop(i);
        }
    }

private:
    struct Q
    {
        MidiEvent m;
        uint32_t due;
    };
    Q q_[32];
    size_t qN_{0};
    void emit(const MidiEvent &e)
    {
        uint8_t st = (e.on ? 0x90 : 0x80) | ((e.ch - 1) & 0x0F);
        Serial1.write(st);
        Serial1.write(e.pitch);
        Serial1.write(e.vel);
    }
    void enqueue(const MidiEvent &e)
    {
        if (qN_ >= 32)
            return;
        q_[qN_++] = {e, (uint32_t)(micros() + e.delay_us)};
    }
    void pop(size_t i)
    {
        for (size_t k = i + 1; k < qN_; ++k)
            q_[k - 1] = q_[k];
        qN_--;
    }
};