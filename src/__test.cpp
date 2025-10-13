// #include <Arduino.h>
// #include <Wire.h>

// static const uint8_t PCF_ADDR = 0x20;
// // Try one set first; if no joy, swap as in step 2.
// static const uint8_t ROW[3] = {10, 11, 12};
// static const uint8_t COL[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// #define COL_N (sizeof(COL) / sizeof(COL[0]))
// #define ROW_N (sizeof(ROW) / sizeof(ROW[0]))

// static inline void pcfWrite16(uint16_t v)
// {
//     Wire.beginTransmission(PCF_ADDR);
//     Wire.write(uint8_t(v & 0xFF));
//     Wire.write(uint8_t((v >> 8) & 0xFF));
//     Wire.endTransmission();
// }
// static inline uint16_t pcfRead16()
// {
//     Wire.requestFrom(PCF_ADDR, (uint8_t)2);
//     uint8_t lo = Wire.read();
//     uint8_t hi = Wire.read();
//     return (uint16_t)lo | ((uint16_t)hi << 8);
// }

// void setup()
// {
//     Serial.begin(115200);
//     while (!Serial && millis() < 1500)
//     {
//     }
//     Serial.println("PCF8575 raw scan");

//     Wire.begin();
//     Wire.setClock(400000);
//     pcfWrite16(0xFFFF); // release all
//     delay(5);

//     // Baseline read with no row driven
//     uint16_t idle = pcfRead16();
//     Serial.printf("IDLE raw=0x%04X (expect bits=1)\n", idle);
// }

// void loop()
// {
//     for (uint8_t r = 0; r < ROW_N; ++r)
//     {
//         uint16_t v = 0xFFFF;           // all inputs
//         v &= ~(uint16_t(1) << ROW[r]); // drive row r low
//         pcfWrite16(v);
//         delayMicroseconds(80);
//         uint16_t raw = pcfRead16();
//         pcfWrite16(0xFFFF); // release back

//         Serial.printf("Row%u raw=0x%04X\n", r, raw);

//         for (uint8_t c = 0; c < COL_N; ++c)
//         {
//             bool pressed = (((raw >> COL[c]) & 1u) == 0);
//             if (pressed)
//                 Serial.printf("  PRESS r%u c%u\n. COL=%u ROW=%u\n", r, c, COL[c], ROW[r]);
//         }
//     }
//     delay(100);
// }
