#include <Arduino.h>
#include <Wire.h>

#include "model/pattern.hpp"
#include "model/viewport.hpp"

#include "core/runloop.hpp"
#include "core/tick_scheduler.hpp"
#include "core/transport.hpp"
#include "core/midi_io.hpp"
#include "engine/playback_engine.hpp"

#include "ui/renderer_oled.hpp"
#include "ui/views/performance_view.hpp"
#include "ui/cursor/serial_keyboard_input.hpp"

static constexpr uint16_t PPQN = 96;
static inline uint32_t ticksPerStep(uint16_t gridDiv) { return (uint32_t(PPQN) * 4u) / gridDiv; }

TickScheduler sched;
Transport transport;
PlaybackEngine engine;
MidiIO midi;
RunLoop runner;
Pattern pat;
Viewport vp;
OledRenderer oled;
PerformanceView perf;

// static uint32_t rng = 0xC0FFEEu;
// static inline uint32_t rnd()
// {
//   rng = 1664525u * rng + 1013904223u;
//   return rng;
// }

static uint16_t visSteps = 16; // S ∈ {1,2,4,8,16,32,64}
// // static uint32_t t0ms = 0;

// static void regenNotes()
// {
//   pat.track.clear();
//   // 64 steps over ~10 octaves (24..127), ~140 notes
//   const uint32_t stepT = ticksPerStep(pat.grid);
//   for (uint16_t s = 0; s < pat.steps; s += (1 + (rnd() % 4)))
//   { // sparse
//     uint8_t reps = 1 + (rnd() % 3);
//     for (uint8_t r = 0; r < reps; r++)
//     {
//       uint8_t pitch = 24 + (rnd() % 104);              // 24..127
//       uint32_t on = s * stepT + (rnd() % (stepT / 2)); // slight offset
//       uint32_t dur = stepT * (1 + (rnd() % 2));        // 1–2 steps
//       uint8_t vel = 60 + (rnd() % 68);

//       Note n{};
//       n.on = on;
//       n.duration = dur;
//       n.pitch = pitch;
//       n.vel = vel;
//       pat.track.notes.push_back(n);
//     }
//   }
// }

// static void handleKeys()
// {
//   static char cmd[16];
//   static uint8_t n = 0;
//   while (Serial.available())
//   {
//     char c = Serial.read();
//     // instant transport
//     if (c == 'P' || c == 'p')
//     {
//       transport.start();
//       midi.sendStart();
//       continue;
//     }
//     if (c == 'S')
//     {
//       transport.stop();
//       midi.sendStop();
//       continue;
//     }
//     if (c == 'R' || c == 'r')
//     {
//       transport.resume();
//       midi.sendContinue();
//       continue;
//     }

//     // utility buttons
//     if (c == '+' || c == '=')
//     {
//       perf.octaveUp();
//       continue;
//     }
//     if (c == '-' || c == '_')
//     {
//       perf.octaveDown();
//       continue;
//     }
//     if (c == 'm' || c == 'M')
//     {
//       perf.cycleMode();
//       continue;
//     }
//     if (c == 'h' || c == 'H')
//     {
//       perf.toggleHold();
//       continue;
//     }
//     if (c == ';')
//     {
//       perf.panic(midi);
//       continue;
//     }

//     // instant nav (don’t buffer)
//     uint32_t stepT = ticksPerStep(pat.grid);
//     switch (c)
//     {
//     case 'a':
//       vp.pan_ticks(-(int32_t)stepT, pat.ticks());
//       continue;
//     case 'A':
//       vp.pan_ticks(-(int32_t)(stepT * 4), pat.ticks());
//       continue;
//     case 'd':
//       vp.pan_ticks((int32_t)stepT, pat.ticks());
//       continue;
//     case 'D':
//       vp.pan_ticks((int32_t)(stepT * 4), pat.ticks());
//       continue;
//       // DEBUG: send test note
//     case 'n':
//       midi.sendNoteNow(pat.track.channel, 60, 100, true);
//       midi.send(MidiEvent{pat.track.channel, 60, 0, false, 200000}); // off in 200ms
//       continue;
//     case 'w':
//       if (visSteps > 1)
//       {
//         visSteps /= 2;
//         vp.zoom_ticks(2.0f, pat.ticks());
//       }
//       continue;
//     case 's':
//       if (visSteps < 64)
//       {
//         visSteps *= 2;
//         vp.zoom_ticks(0.5f, pat.ticks());
//       }
//       continue;
//     case 'z':
//       vp.pan_pitch(-1);
//       continue;
//     case 'x':
//       vp.pan_pitch(+1);
//       continue;
//     case 'Z':
//       vp.pan_pitch(-12);
//       continue;
//     case 'X':
//       vp.pan_pitch(+12);
//       continue;
//     case 'r':
//       regenNotes();
//       continue;
//     default:
//       break;
//     }

//     // line-based commands: T<float>, C<int>, G<int>, L<uint>
//     if (c == '\r' || c == '\n')
//     {
//       if (n)
//       {
//         cmd[n] = 0;
//         char op = cmd[0];
//         switch (op)
//         {
//         case 'T':
//         {
//           float bpm = atof(cmd + 1);
//           if (bpm >= 20 && bpm <= 300)
//           {
//             pat.tempo = bpm;
//             transport.setTempo(bpm);
//           }
//           else
//             Serial.println("ERR bpm 20..300");
//         }
//         break;
//         case 'C':
//         {
//           int ch = atoi(cmd + 1);
//           if (ch >= 1 && ch <= 16)
//             pat.track.channel = ch;
//           else
//             Serial.println("ERR ch 1..16");
//         }
//         break;
//         case 'G':
//         {
//           uint16_t steps = (uint16_t)atoi(cmd + 1);
//           if (steps > 0 && steps <= 256)
//           {
//             pat.steps = steps;
//             transport.setLoopLen(pat.ticks());
//             vp.clamp(pat.ticks());
//           }
//           else
//             Serial.println("ERR steps 1..256");
//         }
//         break;
//         case 'L':
//         {
//           uint32_t t = strtoul(cmd + 1, nullptr, 10);
//           transport.locate(t);
//         }
//         break;
//         default:
//           Serial.println("ERR unknown cmd");
//           break;
//         }
//         n = 0;
//       }
//       continue;
//     }

//     // start/append numeric command buffer
//     if (n == 0)
//     {
//       if (c == 'T' || c == 'C' || c == 'G' || c == 'L')
//         cmd[n++] = c;
//     }
//     else if (isdigit((unsigned char)c) || c == '.' || c == '-' || c == ' ')
//     {
//       if (n < sizeof(cmd) - 1)
//         cmd[n++] = c;
//     }
//     else
//     {
//       n = 0; // invalid char → reset
//     }
//   }
// }

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 1500)
  {
  }

  ///////////////
  Serial.println("MIDI Sequencer. Initing...");
  // Wire.begin();
  // Wire.setClock(100000);
  // Serial.println("I2C scan:");
  // for (uint8_t addr = 1; addr < 127; ++addr)
  // {
  //   Wire.beginTransmission(addr);
  //   if (Wire.endTransmission() == 0)
  //   {
  //     Serial.printf("  Found 0x%02X\n", addr);
  //   }
  // }
  ////////////

  oled.begin();
  midi.begin();
  sched.begin();

  pat.grid = 16;
  pat.steps = 64;
  pat.tempo = 120.f;
  pat.track.channel = 13;

  transport.setLoopLen(pat.ticks());
  transport.setTempo(pat.tempo);
  transport.locate(0);
  transport.start();

  vp.pitchBase = 36;
  vp.tickSpan = ticksPerStep(pat.grid) * visSteps;

  runner.begin(&sched, &transport, &engine, &midi, &pat);
  perf.begin(pat.track.channel);

  // regenNotes();
}

void loop()
{
  // handleKeys();

  // Run clock & transport and generate events
  runner.service();

  // Keyboard input
  perf.poll(midi);

  // Draw
  static uint32_t nextDraw = 0;
  uint32_t now = micros();
  if ((int32_t)(now - nextDraw) >= 0)
  {

    perf.draw(pat, vp, oled, midi, now, transport.playTick());
    nextDraw = now + 50000; // ~20 FPS
  }
}
