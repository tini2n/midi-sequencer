#pragma once
#include <Arduino.h>
#include "core/midi_io.hpp"

class SerialKB
{
public:
    void begin(uint8_t root = 0, int8_t octave = 4, uint8_t vel = 100)
    {
        setRoot(root);
        setOctave(octave);
        setVelocity(vel);
        reset();
    }
    void setRoot(uint8_t r){ root_=r%12; computeBottomOffsets(); }
    void setOctave(int8_t o)
    {
        if (o < 0)
            o = 0;
        if (o > 10)
            o = 10;
        octave_ = o;
    }
    void setVelocity(uint8_t v) { vel_ = v; }
    void reset()
    {
        for (int i = 0; i < 16; i++)
        {
            pressed_[i] = false;
            pitch_[i] = -1;
        }
    }

    // Poll serial; on each recognized key, toggle NoteOn/Off immediately
    void poll(MidiIO &midi, uint8_t channel, int *lastPitchOpt = nullptr)
    {
        while (Serial.available())
        {
            char c = (char)Serial.read();

            int btn = charToBtn(c);
            if (btn < 0)
                continue;

            int p = btnToPitch(btn);
            char name[6]; pitchName((uint8_t)p, name, sizeof(name));

            if (p < 0)
                continue;

            if (!pressed_[btn])
            {
                pressed_[btn] = true;
                pitch_[btn] = p;
                midi.send({channel, (uint8_t)p, vel_, true, 0});
                if (lastPitchOpt)
                    *lastPitchOpt = p;

                Serial.printf("NoteOn %s p%d v%u\n", name, p, vel_);
            }
            else
            {
                midi.send({channel, (uint8_t)pitch_[btn], 0, false, 0});
                pressed_[btn] = false;
                pitch_[btn] = -1;
            }
        }
    }

private:
    // map serial char → 0..15 (top 0..7 = '1'..'8', bottom 8..15 = 'q'..'i')
    static int charToBtn(char c)
    {
        static const char *keys = "12345678qwertyuiQWERTYUI";
        const char *p = strchr(keys, c);
        if (!p)
            return -1;
        int i = int(p - keys);
        return (i < 8) ? i : 8 + ((i - 8) % 8);
    }

    static void pitchName(uint8_t p, char *buf, size_t bufLen)
    {
        static const char *names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        if (p > 127 || bufLen < 3)
        {
            if (bufLen > 0)
                buf[0] = '\0';
            return;
        }
        uint8_t n = p % 12;
        int8_t o = (p / 12) - 1;
        snprintf(buf, bufLen, "%s%d", names[n], o);
    }

    void computeBottomOffsets()
    {
        // map semitone→natural letter index (C=0..B=6), sharps map down to preceding natural
        static const uint8_t natIdx[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
        static const uint8_t gapsC[7] = {2, 2, 1, 2, 2, 2, 1}; // C,D,E,F,G,A,B intervals
        uint8_t li = natIdx[root_];
        bottom_[0] = 0;
        uint8_t acc = 0;
        for (int i = 0; i < 7; i++)
        {
            acc += gapsC[(i + li) % 7];
            bottom_[i + 1] = acc;
        } // offsets for 8 naturals
    }

    int btnToPitch(uint8_t btn) const
    {
        int base = 12 * octave_ + root_;

        if (btn >= 8)
        {
            uint8_t k = btn - 8;
            return clamp(base + bottom_[k]);
        } // q..i naturals

        if (btn == 0)
            return -1; // no black before first

        uint8_t g = btn - 1;

        if (g >= 7)
            return -1; // gap 0..6

        uint8_t diff = bottom_[g + 1] - bottom_[g];

        if (diff == 2)
            return clamp(base + bottom_[g] + 1); // whole-tone gap → black

        return -1;
    }
    static int clamp(int p) { return p < 0 ? 0 : (p > 127 ? 127 : p); }

    bool pressed_[16];
    int16_t pitch_[16];
    uint8_t root_{0}, vel_{100};
    int8_t octave_{4};
    uint8_t bottom_[8]{0}; // bottom row natural key offsets from root
};
