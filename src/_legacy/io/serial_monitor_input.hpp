#pragma once
#include <Arduino.h>

#include "types.hpp"
#include "core/runloop.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"
#include "core/timebase.hpp"
#include "model/pattern.hpp"
#include "model/viewport.hpp"
#include "ui/views/performance_view.hpp"
#include "ui/views/generative_view.hpp"
#include "ui/views/view_manager.hpp"

// Lightweight Serial Monitor input for ghost control during development.
// Reads single-key commands and simple line commands from USB Serial.
// Safe to run in parallel with MatrixKB and the rest of the system.
class SerialMonitorInput
{
public:
    void attach(RunLoop *rl, Transport *tx, Pattern *pat, Viewport *vp, ViewManager *vm, PerformanceView *perf)
    {
        rl_ = rl;
        tx_ = tx;
        pat_ = pat;
        vp_ = vp;
        vm_ = vm;
        perf_ = perf;
    }

    void poll(MidiIO &midi)
    {
        if (!rl_ || !tx_ || !pat_ || !vp_ || !vm_ || !perf_)
            return;

        while (Serial.available())
        {
            char c = (char)Serial.read();

            // Generative commands (when in generative view)
            if (vm_->getCurrentViewType() == ViewType::Generative)
            {
                if (c == 'g' || c == 'G')
                {
                    // Generate pattern
                    GenerativeView *genView = static_cast<GenerativeView *>(vm_->getCurrentView());
                    if (genView)
                    {
                        genView->triggerGeneration(*pat_);
                        Serial.println("Pattern generated");
                    }
                    continue;
                }
                if (c == 'n' || c == 'N')
                {
                    // Next generator
                    GenerativeView *genView = static_cast<GenerativeView *>(vm_->getCurrentView());
                    if (genView)
                    {
                        genView->switchToNextGenerator();
                    }
                    continue;
                }
                if (c == 'b' || c == 'B')
                {
                    // Previous generator (back)
                    GenerativeView *genView = static_cast<GenerativeView *>(vm_->getCurrentView());
                    if (genView)
                    {
                        genView->switchToPreviousGenerator();
                    }
                    continue;
                }
                if (c == 'l' || c == 'L')
                {
                    // List generators
                    GenerativeView *genView = static_cast<GenerativeView *>(vm_->getCurrentView());
                    if (genView)
                    {
                        genView->getGeneratorManager().printAvailableGenerators();
                    }
                    continue;
                }
                if (c == 'i' || c == 'I')
                {
                    // Generator info
                    GenerativeView *genView = static_cast<GenerativeView *>(vm_->getCurrentView());
                    if (genView)
                    {
                        genView->getGeneratorManager().printCurrentGenerator();
                    }
                    continue;
                }
                if (c == 'r' || c == 'R')
                {
                    // Reset to defaults
                    GenerativeView *genView = static_cast<GenerativeView *>(vm_->getCurrentView());
                    if (genView)
                    {
                        genView->resetToDefaults();
                        Serial.println("Reset generator to defaults");
                    }
                    continue;
                }
            }

            // Quick utilities - Octave and Root controls
            if (c == '+')
            {
                // octave up
                int8_t o = perf_->getOctave();
                if (o < 10)
                    o++;
                perf_->setOctave(o);
                Serial.printf("Octave %d\n", (int)o);
                continue;
            }
            if (c == '-')
            {
                // octave down
                int8_t o = perf_->getOctave();
                if (o > 0)
                    o--;
                perf_->setOctave(o);
                Serial.printf("Octave %d\n", (int)o);
                continue;
            }
            if (c == '=')
            {
                // root up
                uint8_t r = perf_->getRoot();
                if (r < 11)
                    r++;
                else
                    r = 0; // wrap around
                perf_->setRoot(r);
                Serial.printf("Root %d\n", (int)r);
                continue;
            }
            if (c == '_')
            {
                // root down
                uint8_t r = perf_->getRoot();
                if (r > 0)
                    r--;
                else
                    r = 11; // wrap around
                perf_->setRoot(r);
                Serial.printf("Root %d\n", (int)r);
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
            // Note: In generative view, 'S' followed by Enter will be handled as parameter setting
            // Only handle 'S' scale/fold prefix in Performance view
            if (c == 'S' && vm_->getCurrentViewType() == ViewType::Performance)
            {
                pendingCmd_ = 'S';
                pendingUntil_ = micros() + 500000; // 500 ms window
                Serial.println("S- prefix: waiting for D/L/0/F...");
                continue;
            }
            // If we are waiting for a suffix after 'S'
            if (pendingCmd_ == 'S')
            {
                // Timeout check
                if ((int32_t)(micros() - pendingUntil_) >= 0)
                {
                    pendingCmd_ = 0; // expired
                }
                else
                {
                    if (c == 'D' || c == 'd')
                    {
                        perf_->setScale(Scale::Dorian);
                        perf_->setFold(false);
                        Serial.println("Scale=Dorian");
                        pendingCmd_ = 0;
                        continue;
                    }
                    if (c == 'L' || c == 'l')
                    {
                        perf_->setScale(Scale::Lydian);
                        perf_->setFold(false);
                        Serial.println("Scale=Lydian");
                        pendingCmd_ = 0;
                        continue;
                    }
                    if (c == '0')
                    {
                        perf_->setScale(Scale::None);
                        perf_->setFold(false);
                        Serial.println("Scale=OFF");
                        pendingCmd_ = 0;
                        continue;
                    }
                    if (c == 'F' || c == 'f')
                    {
                        if (perf_->state().scale != 0)
                        {
                            bool nf = !perf_->state().fold;
                            perf_->setFold(nf);
                            Serial.printf("Fold=%s\n", nf ? "ON" : "OFF");
                        }
                        else
                        {
                            Serial.println("Fold ignored (scale OFF)");
                        }
                        pendingCmd_ = 0;
                        continue;
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
            case 'q':
                vp_->pan_pitch(-1);
                continue;
            case 'e':
                vp_->pan_pitch(+1);
                continue;
            case 'Q':
                vp_->pan_pitch(-12);
                continue;
            case 'E':
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
                    case 'P':
                    {
                        // Handle "set param value" commands for generative view
                        // Format: P<paramname> <value>
                        // Examples:
                        //   Pdensity 80    - Set density parameter to 80
                        //   Plength 16     - Set pattern length to 16 steps
                        //   Pvelocity 100  - Set base velocity to 100
                        //   Pvel_range 20  - Set velocity variation to ±20
                        //   Ppitch_range 12 - Set pitch variation to ±12 semitones
                        //   Pbase_note 60  - Set base MIDI note to 60 (C4)
                        //   Pduration 0.5  - Set note duration to 50% of step
                        if (vm_->getCurrentViewType() == ViewType::Generative)
                        {
                            GenerativeView *genView = static_cast<GenerativeView *>(vm_->getCurrentView());
                            if (genView)
                            {
                                // Parse "P<paramname> <value>" format
                                String cmd(cmdBuf_);
                                int spaceIndex = cmd.indexOf(' ');
                                if (spaceIndex > 1)
                                {
                                    String paramName = cmd.substring(1, spaceIndex);
                                    float value = cmd.substring(spaceIndex + 1).toFloat();

                                    if (genView->getGeneratorManager().setParameter(paramName.c_str(), value))
                                    {
                                        Serial.printf("Set %s = %.2f\n", paramName.c_str(), value);
                                    }
                                    else
                                    {
                                        Serial.printf("Unknown parameter: %s\n", paramName.c_str());
                                        Serial.println("Use 'i' to see available parameters");
                                    }
                                }
                                else
                                {
                                    Serial.println("Usage: P<param> <value>");
                                    Serial.println("Examples: Pdensity 80, Plength 16, Pvelocity 100");
                                    Serial.println("Use 'i' command to see all available parameters");
                                }
                            }
                        }
                        else
                        {
                            Serial.println("Parameter setting only works in Generative view");
                            Serial.println("Switch to Generative view first (Matrix KB buttons 3/4/6)");
                        }
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
                if (c == 'T' || c == 'C' || c == 'G' || c == 'L' || c == 'S' || c == 'P')
                    cmdBuf_[bufLen_++] = c;
            }
            else if (isDigit_(c) || c == '.' || c == '-' || c == ' ' ||
                     (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
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

    // Handle generative view commands

    RunLoop *rl_{nullptr};
    Transport *tx_{nullptr};
    Pattern *pat_{nullptr};
    Viewport *vp_{nullptr};
    ViewManager *vm_{nullptr};
    PerformanceView *perf_{nullptr};

    char cmdBuf_[24]{};
    int bufLen_{0};
    // Pending two-key command handling
    char pendingCmd_{0};
    uint32_t pendingUntil_{0};
};
