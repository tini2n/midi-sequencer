#pragma once
#include "../model/pattern.hpp"
#include "../engine/playback_engine.hpp"
#include "tick_scheduler.hpp"
#include "transport.hpp"
#include "midi_io.hpp"

// Application-level events posted from UI or input handlers.
struct AppEvent {
    enum class Type : uint8_t { Play, Stop, Pause, Resume, ToggleSettings } type;
};

// Main service loop. Call service() from Arduino loop() — never from an ISR.
//
// service() flow each iteration:
//   1. Drain TickScheduler → feed each tick to Transport (phase accumulator).
//   2. Drain Transport windows → PlaybackEngine emits MIDI events into fixed buffer.
//   3. Forward events to MidiIO (immediate or delayed).
//   4. MidiIO::update() fires any delayed events that are now due.
class RunLoop {
public:
    void begin(TickScheduler* s, Transport* tr, PlaybackEngine* pe,
               MidiIO* mi, Pattern* pa) {
        sched_ = s; tx_ = tr; eng_ = pe; midi_ = mi; pat_ = pa;
    }

    void post(const AppEvent& e) {
        if (evtN_ < MaxEvt) evtQ_[evtN_++] = e;
    }

    void service() {
        handleAppEvents();

        TickEvent te;
        while (sched_->fetch(te))
            tx_->onTick();

        TickWindow w;
        while (tx_->next(w)) {
            uint8_t count = 0;
            eng_->processTick(w.prev, w.curr, *pat_, evBuf_, count, MaxEvents);
            for (uint8_t i = 0; i < count; ++i)
                midi_->send(evBuf_[i]);

            // Send MIDI clock: 24 clocks per quarter note → every 4 ticks at PPQN=96.
            if (++clkDiv_ == 4) { midi_->sendClock(); clkDiv_ = 0; }
        }

        midi_->update();
    }

    uint32_t playTick() const { return tx_->playTick(); }

private:
    void handleAppEvents() {
        for (uint8_t i = 0; i < evtN_; ++i) {
            switch (evtQ_[i].type) {
            case AppEvent::Type::Play:
                tx_->start();
                eng_->reset();
                midi_->sendStart();
                break;
            case AppEvent::Type::Stop:
                tx_->stop();
                eng_->reset();
                silenceAllTracks();
                midi_->sendStop();
                break;
            case AppEvent::Type::Pause:
                tx_->pause();
                break;
            case AppEvent::Type::Resume:
                tx_->resume();
                midi_->sendContinue();
                break;
            case AppEvent::Type::ToggleSettings:
                settingsToggle_ = true;
                break;
            }
        }
        evtN_ = 0;
    }

public:
    // Returns true (once) when a ToggleSettings event has been received.
    bool consumeSettingsToggle() {
        bool v = settingsToggle_;
        settingsToggle_ = false;
        return v;
    }

    void silenceAllTracks() {
        if (!pat_) return;
        for (uint8_t t = 0; t < MAX_TRACKS; ++t) {
            uint8_t ch = pat_->tracks[t].channel;
            midi_->sendAllNotesOffCC(ch, true);
        }
    }

    TickScheduler*  sched_{};
    Transport*      tx_{};
    PlaybackEngine* eng_{};
    MidiIO*         midi_{};
    Pattern*        pat_{};

    static constexpr uint8_t MaxEvents = 64;
    MidiEvent evBuf_[MaxEvents]{};

    static constexpr uint8_t MaxEvt = 8;
    AppEvent evtQ_[MaxEvt]{};
    uint8_t  evtN_{0};

    uint8_t clkDiv_{0};
    bool    settingsToggle_{false};
};
