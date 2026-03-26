#pragma once
#include <Arduino.h>

// A single MIDI event passed through the playback pipeline.
struct MidiEvent {
    uint8_t  ch;        // MIDI channel 1–16
    uint8_t  pitch;     // MIDI note number 0–127
    uint8_t  vel;       // velocity 0–127 (0 = note off)
    bool     on;        // true = note-on, false = note-off
    uint32_t delay_us;  // microseconds to wait before sending (0 = immediate)
};

// MIDI output driver for Serial1 (DIN-5 at 31250 baud).
// send() emits immediately or enqueues for timed delivery.
// update() must be called each RunLoop iteration to drain the delay queue.
//
// Delay queue is a fixed FIFO (max 32 entries). Events are assumed to be enqueued
// in non-decreasing due-time order — which is always true because delay_us is small
// and micros() is monotonic within a service() call.
class MidiIO {
public:
    void begin() { Serial1.begin(31250); }

    void send(const MidiEvent& e) {
        if (e.delay_us) enqueue(e);
        else            emit(e);
    }

    // Drain delayed events whose due time has passed. Call once per RunLoop iteration.
    void update() {
        uint32_t now = micros();
        Entry e;
        while (peekFront(e)) {
            if ((int32_t)(now - e.dueUs) < 0) break; // not due yet
            popFront();
            emit(e.ev);
        }
    }

    // Send individual note-off for every pitch on a channel (belt-and-suspenders silence).
    // 128 writes × 3 bytes = 384 bytes total — called only on stop, not in the hot path.
    void allNotesOff(uint8_t ch) {
        for (uint8_t n = 0; n < 128; ++n)
            emit(MidiEvent{ch, n, 0, false, 0});
    }

    // MIDI CC 123 (All Notes Off) + optional CC 120 (All Sound Off).
    void sendAllNotesOffCC(uint8_t ch, bool soundOff = true) {
        uint8_t st = 0xB0 | ((ch - 1) & 0x0F);
        Serial1.write(st); Serial1.write(123); Serial1.write(0);
        if (soundOff) { Serial1.write(st); Serial1.write(120); Serial1.write(0); }
    }

    void sendClock()    { Serial1.write(0xF8); }
    void sendStart()    { Serial1.write(0xFA); }
    void sendContinue() { Serial1.write(0xFB); }
    void sendStop()     { Serial1.write(0xFC); }

private:
    void emit(const MidiEvent& e) {
        uint8_t st = (e.on ? 0x90 : 0x80) | ((e.ch - 1) & 0x0F);
        Serial1.write(st);
        Serial1.write(e.pitch);
        Serial1.write(e.vel);
    }

    // Fixed FIFO delay queue — O(1) push and pop, no shifting.
    struct Entry { MidiEvent ev; uint32_t dueUs; };
    static constexpr uint8_t QCap = 32;
    Entry   q_[QCap];
    uint8_t qHead_{0};  // next write slot
    uint8_t qTail_{0};  // next read slot
    uint8_t qCount_{0};

    void enqueue(const MidiEvent& e) {
        if (qCount_ >= QCap) return; // overflow: drop event
        q_[qHead_] = {e, micros() + e.delay_us};
        qHead_ = (qHead_ + 1) & (QCap - 1);
        qCount_++;
    }
    bool peekFront(Entry& e) const {
        if (!qCount_) return false;
        e = q_[qTail_];
        return true;
    }
    void popFront() {
        if (!qCount_) return;
        qTail_ = (qTail_ + 1) & (QCap - 1);
        qCount_--;
    }
};
