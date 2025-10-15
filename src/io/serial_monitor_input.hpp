#pragma once
#include <Arduino.h>

#include "types.hpp"
#include "core/runloop.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"
#include "model/pattern.hpp"
#include "model/viewport.hpp"
#include "ui/views/performance_view.hpp"

// Lightweight Serial Monitor input for ghost control during development.
// Reads single-key commands and simple line commands from USB Serial.
// Safe to run in parallel with MatrixKB and the rest of the system.
class SerialMonitorInput
{
public:
    void attach(RunLoop *rl, Transport *tx, Pattern *pat, Viewport *vp, PerformanceView *perf)
    {
        rl_ = rl;
        tx_ = tx;
        pat_ = pat;
        vp_ = vp;
        perf_ = perf;
    }

    void poll(MidiIO &midi)
    {
        if (!rl_ || !tx_ || !pat_ || !vp_ || !perf_)
            return;

        while (Serial.available())
        {
            char c = (char)Serial.read();

            // Transport (instant, via RunLoop events)
            if (c == 'P' || c == 'p')
            {
                rl_->post(AppEvent{AppEvent::Type::Play});
                Serial.println("Play");
                continue;
            }
            if (c == 'R' || c == 'r')
            {
                rl_->post(AppEvent{AppEvent::Type::Resume});
                Serial.println("Resume");
                continue;
            }

            // Quick utilities
            if (c == '+' || c == '=')
            {
                // octave up
                int8_t o = perf_->state().octave;
                if (o < 10)
                    o++;
                perf_->setOctave(o);
                Serial.printf("Octave %d\n", (int)o);
                continue;
            }
            if (c == '-' || c == '_')
            {
                // octave down
                int8_t o = perf_->state().octave;
                if (o > 0)
                    o--;
                perf_->setOctave(o);
                Serial.printf("Octave %d\n", (int)o);
                continue;
            }
            if (c == ';')
            {
                // Panic: send NoteOff for all notes on current channel
                uint8_t ch = perf_->state().channel;
                for (uint8_t n = 0; n < 128; ++n)
                    midi.send(MidiEvent{ch, n, 0, false, 0});
                Serial.println("PANIC sent (all notes off)\n");
                continue;
            }

            // Scale and fold quick commands
            // Two-character commands with a tolerant entry model:
            // 1) Type both: SD / SL / S0 / SF (Enter not required)
            // 2) Or type 'S' then 'D/L/0/F' within ~500ms (no need to press together)
            if (c == 'S') {
                pendingCmd_ = 'S';
                pendingUntil_ = micros() + 500000; // 500 ms window
                Serial.println("S- prefix: waiting for D/L/0/F...");
                continue;
            }
            // If we are waiting for a suffix after 'S'
            if (pendingCmd_ == 'S') {
                // Timeout check
                if ((int32_t)(micros() - pendingUntil_) >= 0) {
                    pendingCmd_ = 0; // expired
                } else {
                    if (c == 'D' || c == 'd') { perf_->setScale(Scale::Dorian); perf_->setFold(false); Serial.println("Scale=Dorian"); pendingCmd_ = 0; continue; }
                    if (c == 'L' || c == 'l') { perf_->setScale(Scale::Lydian); perf_->setFold(false); Serial.println("Scale=Lydian"); pendingCmd_ = 0; continue; }
                    if (c == '0')            { perf_->setScale(Scale::None);   perf_->setFold(false); Serial.println("Scale=OFF");    pendingCmd_ = 0; continue; }
                    if (c == 'F' || c == 'f') {
                        if (perf_->state().scale != 0) {
                            bool nf = !perf_->state().fold; perf_->setFold(nf); Serial.printf("Fold=%s\n", nf?"ON":"OFF");
                        } else {
                            Serial.println("Fold ignored (scale OFF)");
                        }
                        pendingCmd_ = 0; continue;
                    }
                    // Any other char -> ignore and keep waiting (until timeout)
                }
            }

            // Viewport navigation (instant)
            uint32_t stepT = timebase::ticksPerStep(pat_->grid);
            switch (c)
            {
            case 'a':
                vp_->pan_ticks(-(int32_t)stepT, pat_->ticks());
                continue;
            case 'A':
                vp_->pan_ticks(-(int32_t)(stepT * 4), pat_->ticks());
                continue;
            case 'd':
                vp_->pan_ticks((int32_t)stepT, pat_->ticks());
                continue;
            case 'D':
                vp_->pan_ticks((int32_t)(stepT * 4), pat_->ticks());
                continue;
            case 'w':
                vp_->zoom_ticks(2.0f, pat_->ticks());
                continue;
            case 's':
                vp_->zoom_ticks(0.5f, pat_->ticks());
                continue;
            case 'z':
                vp_->pan_pitch(-1);
                continue;
            case 'x':
                vp_->pan_pitch(+1);
                continue;
            case 'Z':
                vp_->pan_pitch(-12);
                continue;
            case 'X':
                vp_->pan_pitch(+12);
                continue;
            default:
                break;
            }

            // Line-based commands: T<float>, C<int>, G<int>, L<uint>
            if (c == '\r' || c == '\n')
            {
                if (bufLen_)
                {
                    cmdBuf_[bufLen_] = 0;
                    char op = cmdBuf_[0];
                    switch (op)
                    {
                    case 'T':
                    {
                        float bpm = atof(cmdBuf_ + 1);
                        if (bpm >= 20 && bpm <= 300)
                        {
                            pat_->tempo = bpm;
                            tx_->setTempo(bpm);
                            Serial.printf("Tempo=%.2f\n", bpm);
                        }
                        else
                        {
                            Serial.println("ERR bpm 20..300");
                        }
                    }
                    break;
                    case 'C':
                    {
                        int ch = atoi(cmdBuf_ + 1);
                        if (ch >= 1 && ch <= 16)
                        {
                            pat_->track.channel = (uint8_t)ch;
                            perf_->state().channel = (uint8_t)ch;
                            Serial.printf("Channel=%d\n", ch);
                        }
                        else
                        {
                            Serial.println("ERR ch 1..16");
                        }
                    }
                    break;
                    case 'G': // steps
                    {
                        uint16_t steps = (uint16_t)atoi(cmdBuf_ + 1);
                        if (steps > 0 && steps <= 256)
                        {
                            pat_->steps = steps;
                            tx_->setLoopLen(pat_->ticks());
                            vp_->clamp(pat_->ticks());
                            Serial.printf("Steps=%u (ticks=%lu)\n", steps, (unsigned long)pat_->ticks());
                        }
                        else
                        {
                            Serial.println("ERR steps 1..256");
                        }
                    }
                    break;
                    // Description: Move playhead to specified tick position.
                    // Note: no range check; user is responsible to provide valid tick within pattern length
                    // (pattern length can be queried with 'G' command)
                    case 'L':
                    {
                        uint32_t t = strtoul(cmdBuf_ + 1, nullptr, 10);
                        tx_->locate(t);
                        Serial.printf("Locate=%lu\n", (unsigned long)t);
                    }
                    break;
                    default:
                        Serial.println("ERR unknown cmd");
                        break;
                    }
                    bufLen_ = 0;
                }
                continue;
            }

            // Start/append numeric command buffer
            if (bufLen_ == 0)
            {
                if (c == 'T' || c == 'C' || c == 'G' || c == 'L')
                    cmdBuf_[bufLen_++] = c;
            }
            else if (isDigit_(c) || c == '.' || c == '-' || c == ' ')
            {
                if (bufLen_ < (int)sizeof(cmdBuf_) - 1)
                    cmdBuf_[bufLen_++] = c;
            }
            else
            {
                // invalid char → reset buffer
                bufLen_ = 0;
            }
        }
    }

private:
    static bool isDigit_(char c) { return c >= '0' && c <= '9'; }

    RunLoop *rl_{nullptr};
    Transport *tx_{nullptr};
    Pattern *pat_{nullptr};
    Viewport *vp_{nullptr};
    PerformanceView *perf_{nullptr};

    char cmdBuf_[24]{};
    int bufLen_{0};
    // Pending two-key command handling
    char pendingCmd_{0};
    uint32_t pendingUntil_{0};
};
