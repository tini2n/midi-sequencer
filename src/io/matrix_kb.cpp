#include "matrix_kb.hpp"
#include "cursor_mode.hpp"

void MatrixKB::poll(MidiIO& midi, uint8_t ch) {
    uint32_t now = micros();

    for (uint8_t r = 0; r < 3; r++) {
        if (!driveRow(r)) continue;

        uint16_t pins = 0xFFFF;
        if (!pcf_.read(pins)) continue;

        for (uint8_t c = 0; c < 8; c++) {
            bool down = ((pins >> cfg_.cols[c]) & 1) == 0;
            int  btn  = rowToBtn(r, c);
            if (btn < 0) continue;

            uint32_t& deb = (r == cfg_.rowCtl) ? debCtlUntil_[c] : debUntil_[btn];
            if ((int32_t)(now - deb) < 0) continue;

            bool last = lastDown_[r][c];
            if (down == last) continue;

            lastDown_[r][c] = down;
            deb = now + cfg_.debounce_us;

            if (r == cfg_.rowCtl) {
                onControl(c, down);
                continue;
            }

            // Delegate to active mode (e.g. CursorMode)
            if (mode_) {
                if (down) mode_->onButtonDown((uint8_t)btn, midi, ch, modeCtx_);
                else      mode_->onButtonUp((uint8_t)btn,   midi, ch, modeCtx_);
                continue;
            }

            // Default: piano key behaviour
            if (down) noteOn(btn, midi, ch);
            else      noteOff(btn, midi, ch);
        }
    }

    pcf_.write(0xFFFF); // restore all pins as inputs
}

bool MatrixKB::driveRow(uint8_t r) {
    uint16_t v = 0xFFFF;
    v &= ~(1u << cfg_.rows[r]);
    return pcf_.write(v);
}

int MatrixKB::rowToBtn(uint8_t r, uint8_t c) const {
    if (r == cfg_.rowTop) return c;
    if (r == cfg_.rowBot) return 8 + c;
    if (r == cfg_.rowCtl) return 16 + c;
    return -1;
}

void MatrixKB::onControl(uint8_t c, bool down) {
#ifdef SEQUENCER_DEBUG
    Serial.printf("[CTL] %u %s\n", c, down ? "down" : "up");
#endif
    // Let mode handle it first (shift key, copy/paste, etc.)
    bool shift = false;
    if (mode_) {
        // Query shift state from CursorMode if available
        CursorMode* cm = static_cast<CursorMode*>(mode_);
        shift = cm ? cm->isShiftPressed() : false;
        if (mode_->onControl(c, down, shift, modeCtx_)) return;
    }

    if (!down) return;

    CursorMode* cm  = mode_ ? static_cast<CursorMode*>(mode_) : nullptr;
    Pattern*    pat = static_cast<Pattern*>(modeCtx_);

    switch (c) {
    case 0: // REC — placeholder until real-time recording is implemented
        Serial.println("[CTL] REC (not implemented)");
        break;
    case 1: // PLAY / pause toggle
        if (rl_ && tx_) {
            bool running = tx_->isRunning();
            rl_->post(AppEvent{running ? AppEvent::Type::Pause : AppEvent::Type::Play});
        }
        break;
    case 2: // STOP
        if (rl_) rl_->post(AppEvent{AppEvent::Type::Stop});
        break;
    case 3: // NEXT PAGE (Shift = prev page)
        if (cm && pat) {
            uint8_t page = cm->getPage();
            if (shift) cm->setPage(page > 0 ? page - 1 : 0, pat->steps);
            else        cm->setPage(page + 1, pat->steps);
        }
        break;
    case 4: // MODE — cycle keyboard mode (only sequencer mode for now)
        Serial.println("[CTL] MODE: sequencer");
        break;
    case 5: // TRACK — toggle active track 0↔1
        if (cm) {
            uint8_t next = (cm->getTrack() + 1) % MAX_TRACKS;
            cm->setTrack(next);
            Serial.printf("[CTL] track -> %u\n", next);
        }
        break;
    case 6: // SETTINGS — toggle settings mode via RunLoop event
        if (rl_) rl_->post(AppEvent{AppEvent::Type::ToggleSettings});
        break;
    // case 7: SHIFT — handled above by onControl(); never reaches here
    default:
        break;
    }
}

void MatrixKB::noteOn(int btn, MidiIO& midi, uint8_t ch) {
    int p = btnToPitch(btn);
    if (p < 0 || pressed_[btn]) return;
    pressed_[btn] = true;
    pitch_[btn]   = p;
    midi.send({ch, (uint8_t)p, vel_, true, 0});
#ifdef SEQUENCER_DEBUG
    Serial.printf("[MIDI] ON  p%d btn%u ch%u v%u\n", p, (uint8_t)btn, ch, vel_);
#endif
}

void MatrixKB::noteOff(int btn, MidiIO& midi, uint8_t ch) {
    if (!pressed_[btn]) return;
    int p = pitch_[btn];
    pressed_[btn] = false;
    pitch_[btn]   = -1;
    if (p >= 0 && p <= 127) {
        midi.send({ch, (uint8_t)p, 0, false, 0});
#ifdef SEQUENCER_DEBUG
        Serial.printf("[MIDI] OFF p%d btn%u ch%u\n", p, (uint8_t)btn, ch);
#endif
    }
}

void MatrixKB::computeBottomOffsets() {
    static const uint8_t natIdx[12] = {0,0,1,1,2,3,3,4,4,5,5,6};
    static const uint8_t gapsC[7]   = {2,2,1,2,2,2,1};
    uint8_t li = natIdx[root_];
    bot_[0] = 0;
    uint8_t acc = 0;
    for (int i = 0; i < 7; i++) { acc += gapsC[(i + li) % 7]; bot_[i+1] = acc; }
}

int MatrixKB::btnToPitch(uint8_t btn) const {
    int base = 12 * oct_ + root_;
    if (fold_ && scale_ != Scale::None) {
        uint8_t idx   = (btn < 8) ? (btn + 8) : (btn - 8);
        uint8_t deg   = idx % 7;
        uint8_t octUp = idx / 7;
        uint8_t semi  = scale::degreeSemitone(scale_, deg) + 12 * octUp;
        return clamp(base + semi);
    }
    if (btn >= 8) {
        uint8_t k = btn - 8;
        int p = clamp(base + bot_[k]);
        return (scale_ == Scale::None || scale::contains(scale_, root_, (uint8_t)p)) ? p : -1;
    }
    if (btn == 0) return -1;
    uint8_t g = btn - 1;
    if (g >= 7) return -1;
    uint8_t diff = bot_[g+1] - bot_[g];
    if (diff == 2) {
        int p = clamp(base + bot_[g] + 1);
        return (scale_ == Scale::None || scale::contains(scale_, root_, (uint8_t)p)) ? p : -1;
    }
    return -1;
}
