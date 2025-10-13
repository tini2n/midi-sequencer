
// static uint32_t rng = 0xC0FFEEu;
// static inline uint32_t rnd()
// {
//   rng = 1664525u * rng + 1013904223u;
//   return rng;
// }

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

