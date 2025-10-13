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
                continue;
            }
            if (c == 'S')
            {
                rl_->post(AppEvent{AppEvent::Type::Stop});
                continue;
            }
            if (c == 'R' || c == 'r')
            {
                rl_->post(AppEvent{AppEvent::Type::Resume});
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
            if (c == 'n')
            {
                // Debug: blip middle C
                uint8_t ch = perf_->state().channel;
                midi.sendNoteNow(ch, 60, 100, true);
                midi.send(MidiEvent{ch, 60, 0, false, 200000}); // off in 200ms
                continue;
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
                    case 'G':
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
};
