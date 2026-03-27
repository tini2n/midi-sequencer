#pragma once
#include <Arduino.h>
#include "../core/runloop.hpp"
#include "../core/transport.hpp"
#include "../core/midi_io.hpp"
#include "../model/pattern.hpp"
#include "cursor_mode.hpp"

// Lightweight USB serial command interface for testing without hardware UI.
// Call poll() from App::update() — reads USB Serial, executes commands.
//
// Commands (type then press Enter, except single-char commands):
//   ?           — print help
//   p           — play / stop toggle
//   T<bpm>      — set tempo (e.g. T140)
//   G<steps>    — set pattern steps (e.g. G32)
//   A<tr>,<pitch>,<step>  — add note at step (vel=100, dur=1 step)
//   X<tr>,<step>          — remove note at step
//   C  or C<tr>           — clear track (default: track 0)
//   L                     — list all tracks (step grid)
//
// No String, no heap. Line buffer: char buf_[48].
class SerialMonitor {
public:
    void attach(RunLoop* rl, Transport* tx, Pattern* pat, MidiIO* midi, CursorMode* cursor) {
        rl_ = rl; tx_ = tx; pat_ = pat; midi_ = midi; cursor_ = cursor;
    }

    void poll() {
        while (Serial.available()) {
            char c = (char)Serial.read();

            // Single-char commands (no Enter needed)
            if (c == 'p' || c == 'P') {
                if (tx_ && rl_) {
                    bool running = tx_->isRunning();
                    rl_->post(AppEvent{running ? AppEvent::Type::Stop : AppEvent::Type::Play});
                    Serial.println(running ? "STOP" : "PLAY");
                }
                bufLen_ = 0;
                continue;
            }
            if (c == '?') { printHelp(); bufLen_ = 0; continue; }

            // Line-based commands — accumulate until Enter
            if (c == '\r' || c == '\n') {
                if (bufLen_ > 0) {
                    buf_[bufLen_] = '\0';
                    executeCmd(buf_);
                    bufLen_ = 0;
                }
                continue;
            }

            // Accumulate printable chars
            if (bufLen_ < (int)sizeof(buf_) - 1 && c >= 0x20)
                buf_[bufLen_++] = c;
        }
    }

private:
    void executeCmd(const char* s) {
        if (!pat_ || !tx_) return;

        char op = s[0];

        if (op == 'T' || op == 't') {
            float bpm = atof(s + 1);
            if (bpm >= 20.f && bpm <= 300.f) {
                pat_->tempo = bpm;
                tx_->setTempo(bpm);
                Serial.printf("Tempo=%.1f\n", bpm);
            } else { Serial.println("ERR: bpm 20..300"); }
            return;
        }

        if (op == 'G' || op == 'g') {
            int steps = atoi(s + 1);
            if (steps >= 1 && steps <= 255) {
                pat_->steps = (uint8_t)steps;
                tx_->setLoopLen(pat_->ticks());
                Serial.printf("Steps=%d  ticks=%lu\n", steps, (unsigned long)pat_->ticks());
            } else { Serial.println("ERR: steps 1..255"); }
            return;
        }

        if (op == 'L' || op == 'l') {
            if (cursor_) {
                for (uint8_t t = 0; t < MAX_TRACKS; t++) {
                    cursor_->setTrack(t);
                    cursor_->printTrackState(*pat_);
                }
                // Restore track
                cursor_->setTrack(0);
            } else {
                printTracksRaw();
            }
            return;
        }

        if (op == 'C' || op == 'c') {
            uint8_t tr = (s[1] >= '0' && s[1] <= '9') ? (uint8_t)atoi(s + 1) : 0;
            if (tr < MAX_TRACKS) {
                pat_->tracks[tr].recorded.clear();
                pat_->tracks[tr].recorded.sortByOnTick();
                Serial.printf("Cleared track %u\n", tr);
            } else { Serial.printf("ERR: track 0..%u\n", MAX_TRACKS - 1); }
            return;
        }

        // A<tr>,<pitch>,<step>  — add note
        if (op == 'A' || op == 'a') {
            uint8_t tr, pitch, step;
            if (parseCSV3(s + 1, tr, pitch, step)) {
                if (tr < MAX_TRACKS && pitch <= 127 && step < pat_->steps) {
                    if (cursor_) {
                        uint8_t prevTr = cursor_->getTrack();
                        cursor_->setTrack(tr);
                        cursor_->setEditPitch(pitch);
                        // Force add (not toggle — if already exists, skip)
                        NotePool<256>& pool = pat_->tracks[tr].recorded;
                        uint32_t tick = uint32_t(step) * (pat_->ticks() / pat_->steps);
                        bool exists = false;
                        for (uint16_t i = 0; i < pool.count; ++i)
                            if (pool.notes[i].on == tick) { exists = true; break; }
                        if (!exists) {
                            Note n{}; n.on = tick; n.duration = pat_->ticks()/pat_->steps;
                            n.pitch = pitch; n.vel = 100;
                            pool.push(n); pool.sortByOnTick();
                        }
                        cursor_->printTrackState(*pat_);
                        cursor_->setTrack(prevTr);
                    }
                } else { Serial.println("ERR: A<tr>,<pitch>,<step>"); }
            } else { Serial.println("ERR: A<tr>,<pitch>,<step>"); }
            return;
        }

        // X<tr>,<step>  — remove note
        if (op == 'X' || op == 'x') {
            uint8_t tr, step;
            if (parseCSV2(s + 1, tr, step)) {
                if (tr < MAX_TRACKS && step < pat_->steps) {
                    NotePool<256>& pool = pat_->tracks[tr].recorded;
                    uint32_t tick = uint32_t(step) * (pat_->ticks() / pat_->steps);
                    for (uint16_t i = 0; i < pool.count; ) {
                        if (pool.notes[i].on == tick) pool.removeAt(i);
                        else ++i;
                    }
                    if (cursor_) { uint8_t prev = cursor_->getTrack(); cursor_->setTrack(tr); cursor_->printTrackState(*pat_); cursor_->setTrack(prev); }
                    else Serial.printf("Removed note at trk%u step%u\n", tr, step);
                } else { Serial.println("ERR: X<tr>,<step>"); }
            } else { Serial.println("ERR: X<tr>,<step>"); }
            return;
        }

        Serial.printf("ERR: unknown cmd '%s' — type ? for help\n", s);
    }

    void printHelp() const {
        Serial.println("--- Serial Monitor Commands ---");
        Serial.println("  p              Play / stop toggle");
        Serial.println("  T<bpm>         Set tempo  e.g. T140");
        Serial.println("  G<steps>       Set steps  e.g. G32");
        Serial.println("  A<tr>,<pit>,<step>  Add note   e.g. A0,60,0");
        Serial.println("  X<tr>,<step>   Remove note    e.g. X0,4");
        Serial.println("  C  or C<tr>    Clear track    e.g. C0");
        Serial.println("  L              List all tracks");
    }

    void printTracksRaw() const {
        for (uint8_t t = 0; t < MAX_TRACKS; t++) {
            Serial.printf("Trk%u (%u notes):\n", t, pat_->tracks[t].recorded.count);
            for (uint16_t i = 0; i < pat_->tracks[t].recorded.count; i++) {
                const Note& n = pat_->tracks[t].recorded.notes[i];
                Serial.printf("  on=%lu dur=%lu pitch=%u vel=%u\n",
                              (unsigned long)n.on, (unsigned long)n.duration, n.pitch, n.vel);
            }
        }
    }

    // Parse "tr,pitch,step" from s. Returns true on success.
    static bool parseCSV3(const char* s, uint8_t& a, uint8_t& b, uint8_t& c) {
        char tmp[16]; uint8_t i = 0;
        while (s[i] && i < 15) { tmp[i] = s[i]; i++; } tmp[i] = '\0';
        char* p1 = tmp;
        char* p2 = nullptr, *p3 = nullptr;
        for (uint8_t j = 0; j < i; j++) {
            if (tmp[j] == ',') {
                if (!p2) { tmp[j] = '\0'; p2 = tmp + j + 1; }
                else     { tmp[j] = '\0'; p3 = tmp + j + 1; break; }
            }
        }
        if (!p2 || !p3) return false;
        a = (uint8_t)atoi(p1); b = (uint8_t)atoi(p2); c = (uint8_t)atoi(p3);
        return true;
    }

    static bool parseCSV2(const char* s, uint8_t& a, uint8_t& b) {
        char tmp[12]; uint8_t i = 0;
        while (s[i] && i < 11) { tmp[i] = s[i]; i++; } tmp[i] = '\0';
        char* p2 = nullptr;
        for (uint8_t j = 0; j < i; j++) if (tmp[j] == ',') { tmp[j] = '\0'; p2 = tmp + j + 1; break; }
        if (!p2) return false;
        a = (uint8_t)atoi(tmp); b = (uint8_t)atoi(p2);
        return true;
    }

    RunLoop*     rl_{nullptr};
    Transport*   tx_{nullptr};
    Pattern*     pat_{nullptr};
    MidiIO*      midi_{nullptr};
    CursorMode*  cursor_{nullptr};

    char buf_[48]{};
    int  bufLen_{0};
};
