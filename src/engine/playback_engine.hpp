#pragma once
#include <stdint.h>
#include "../model/pattern.hpp"
#include "../core/midi_io.hpp"
#include "../core/timebase.hpp"

// Cursor-based playback engine. O(1) amortized per tick.
//
// Design:
//   - Notes must be sorted by on-tick before playback starts (call NotePool::sortByOnTick()).
//   - Two cursors per track (recorded + generative) advance forward through sorted notes.
//   - Note-offs are tracked in a fixed active-note table and fired when their due tick arrives.
//   - On pattern wrap (curr < prev): cursors reset, active notes are silenced immediately.
//
// Call processTick() once per TickWindow from the RunLoop.
// Output is written into a caller-supplied fixed array — no heap.
class PlaybackEngine {
public:
    void reset() {
        for (uint8_t i = 0; i < MAX_TRACKS; ++i)
            recCursor_[i] = genCursor_[i] = 0;
        activeCount_ = 0;
    }

    void processTick(uint32_t prev, uint32_t curr, const Pattern& pat,
                     MidiEvent* out, uint8_t& outCount, uint8_t maxOut) {
        const bool wrap    = (curr < prev);
        const uint32_t len = pat.ticks();

        if (wrap) {
            // Silence all active notes at the pattern boundary, reset cursors.
            flushActive(out, outCount, maxOut);
            for (uint8_t i = 0; i < MAX_TRACKS; ++i)
                recCursor_[i] = genCursor_[i] = 0;
        }

        // Fire pending note-offs that fall in this tick window.
        processNoteOffs(prev, curr, wrap, out, outCount, maxOut);

        // Advance through each track's recorded and generative layers.
        for (uint8_t t = 0; t < MAX_TRACKS; ++t) {
            const LayeredTrack& trk = pat.tracks[t];
            const uint8_t ch = trk.channel;

            processLayer(trk.recorded,   recCursor_[t], prev, curr, len, ch, pat.tempo, out, outCount, maxOut);
            processLayer(trk.generative, genCursor_[t], prev, curr, len, ch, pat.tempo, out, outCount, maxOut);
        }
    }

private:
    // Per-track cursors into sorted NotePool (recorded and generative layers).
    uint16_t recCursor_[MAX_TRACKS]{};
    uint16_t genCursor_[MAX_TRACKS]{};

    // Pending note-offs for currently sounding notes.
    struct ActiveNote {
        uint8_t  ch;
        uint8_t  pitch;
        uint32_t offTick; // pattern tick when note-off fires
    };
    static constexpr uint8_t MaxActive = 64;
    ActiveNote active_[64]{};
    uint8_t    activeCount_{0};

    // Advance cursor through a sorted NotePool, emitting note-ons for notes in (prev, curr].
    template <size_t N>
    void processLayer(const NotePool<N>& pool, uint16_t& cursor,
                      uint32_t prev, uint32_t curr, uint32_t loopLen,
                      uint8_t ch, float bpm,
                      MidiEvent* out, uint8_t& outCount, uint8_t maxOut) {
        while (cursor < pool.count) {
            const Note& n = pool.notes[cursor];
            if (n.on > curr) break;           // no more notes in window
            if (n.on > prev) {                // note falls in (prev, curr]
                if (!(n.flags & NF_Mute))
                    emitNoteOn(n, ch, bpm, out, outCount, maxOut);
                scheduleNoteOff(ch, n.pitch, (n.on + n.duration) % loopLen);
            }
            cursor++;
        }
    }

    // Check active-note table and fire note-offs whose off-tick falls in this window.
    void processNoteOffs(uint32_t prev, uint32_t curr, bool wrap,
                         MidiEvent* out, uint8_t& outCount, uint8_t maxOut) {
        for (uint8_t i = 0; i < activeCount_; ) {
            uint32_t off = active_[i].offTick;
            bool fire = wrap
                ? (off > prev || off <= curr)   // wrap: (prev, loopEnd) ∪ [0, curr]
                : (off > prev && off <= curr);   // normal: (prev, curr]
            if (fire) {
                pushOut({active_[i].ch, active_[i].pitch, 0, false, 0}, out, outCount, maxOut);
                // Remove by swapping with last entry.
                active_[i] = active_[--activeCount_];
            } else {
                i++;
            }
        }
    }

    // Send note-off for all active notes immediately (used on pattern wrap / stop).
    void flushActive(MidiEvent* out, uint8_t& outCount, uint8_t maxOut) {
        for (uint8_t i = 0; i < activeCount_; ++i)
            pushOut({active_[i].ch, active_[i].pitch, 0, false, 0}, out, outCount, maxOut);
        activeCount_ = 0;
    }

    void emitNoteOn(const Note& n, uint8_t ch, float bpm,
                    MidiEvent* out, uint8_t& outCount, uint8_t maxOut) {
        uint32_t delay = 0;
        if (n.micro_q8 > 0) {
            // Convert sub-tick offset to microseconds.
            uint32_t upt = usPerTick(Tempo{bpm, timebase::PPQN});
            delay = (uint32_t)((int32_t)n.micro_q8 * (int32_t)upt / 256);
        }
        pushOut({ch, n.pitch, n.vel, true, delay}, out, outCount, maxOut);
    }

    void scheduleNoteOff(uint8_t ch, uint8_t pitch, uint32_t offTick) {
        if (activeCount_ >= MaxActive) return; // table full — note-off will be missed
        active_[activeCount_++] = {ch, pitch, offTick};
    }

    void pushOut(const MidiEvent& ev, MidiEvent* out, uint8_t& outCount, uint8_t maxOut) {
        if (outCount < maxOut)
            out[outCount++] = ev;
    }
};
