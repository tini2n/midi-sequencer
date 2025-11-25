#pragma once
#include <Arduino.h>
#include "core/midi_io.hpp"
#include "io/pcf8575.hpp"
#include "core/runloop.hpp"
#include "core/transport.hpp"
#include "engine/record_engine.hpp"
#include "model/pattern.hpp"
#include "io/matrix_kb_mode.hpp"
#include "io/performance_mode.hpp"
#include "io/cursor_mode.hpp"

/**
 * MatrixKB: Hardware controller for 2x8 button matrix via PCF8575.
 * Delegates button interpretation to mode strategies (PerformanceMode / CursorMode).
 * Handles scanning, debouncing, and control row (transport) buttons.
 */
class MatrixKB
{
public:
    enum class Mode
    {
        Performance,
        Cursor
    };

    struct Config
    {
        uint8_t address = 0x20;              // I2C address of PCF8575
        uint8_t rows[3] = {10, 11, 12};      // Row drive pins
        uint8_t cols[8] = {0, 1, 2, 3, 4, 5, 6, 7}; // Column sense pins
        uint8_t rowTop = 1, rowBot = 2, rowCtl = 0; // Row indices
        uint32_t debounce_us = 5000;         // Increased from 3ms to 5ms for stability
    };

    bool begin(const Config &c, uint8_t root = 0, int8_t octave = 4, uint8_t vel = 100)
    {
        cfg_ = c;
        pcf_.begin(cfg_.address);
        
        // Initialize performance mode with defaults
        perfMode_.setRoot(root);
        perfMode_.setOctave(octave);
        perfMode_.setVelocity(vel);
        perfMode_.reset();
        
        // Set default mode
        setMode(Mode::Performance);
        
        // default all pins high (inputs)
        pcf_.write(0xFFFF);
        return true;
    }

    // Mode management
    void setMode(Mode m)
    {
        if (mode_ == m)
            return;
        
        Serial.printf("[MatrixKB] Switching mode: %d -> %d\n", (int)mode_, (int)m);
        
        // Deactivate old mode
        if (currentMode_)
            currentMode_->onDeactivate();
        
        mode_ = m;
        currentMode_ = (m == Mode::Performance) ? static_cast<IMatrixKBMode*>(&perfMode_) 
                                                 : static_cast<IMatrixKBMode*>(&cursorMode_);
        
        // Activate new mode
        if (currentMode_) {
            currentMode_->onActivate();
            Serial.printf("[MatrixKB] Mode activated: %s\n", m == Mode::Performance ? "Performance" : "Cursor");
        }
    }

    Mode getMode() const { return mode_; }

    // Context setters for cursor mode
    void setPattern(Pattern *p) { pattern_ = p; }

    // Performance mode API delegation (for backward compatibility)
    void setRoot(uint8_t r) { perfMode_.setRoot(r); }
    void setOctave(int8_t o) { perfMode_.setOctave(o); }
    void setVelocity(uint8_t v) { perfMode_.setVelocity(v); }
    void setScale(Scale s) { perfMode_.setScale(s); }
    void setFold(bool f) { perfMode_.setFold(f); }
    void reset() { perfMode_.reset(); }

    /**
     * Configure current mode parameters (decoupled from concrete mode types).
     * @param config Configuration structure with mode-specific parameters
     */
    void configureMode(const IModeConfig& config)
    {
        if (currentMode_) {
            currentMode_->configure(config);
        }
    }
    
    /**
     * Read current mode configuration.
     * @param config Output parameter filled with current mode state
     */
    void getModeConfig(IModeConfig& config) const
    {
        if (currentMode_) {
            currentMode_->getConfig(config);
        }
    }

    // Attach external systems for control events and recording hooks
    void attach(RunLoop *rl, RecordEngine *rec, Transport *tx)
    {
        rl_ = rl;
        rec_ = rec;
        tx_ = tx;
        perfMode_.attach(rec, tx);
    }

    // Attach view manager for view switching controls
    void attachViewManager(class ViewManager *vm)
    {
        vm_ = vm;
    }

    void poll(MidiIO &midi, uint8_t ch, int *lastPitchOpt = nullptr)
    {
        uint32_t now = micros();
        
        // Adaptive throttling: 200Hz (5ms) for performance mode, 100Hz (10ms) for cursor mode
        uint32_t scanInterval = (mode_ == Mode::Performance) ? 5000 : 10000;
        if ((int32_t)(now - lastScanUs_) < (int32_t)scanInterval) {
            return;
        }
        lastScanUs_ = now;
        
        // Scan each row
        for (uint8_t r = 0; r < 3; r++)
        {
            // Drive row low, others high
            if (!driveRow(r)) {
                continue;
            }
            
            // Wait for signals to settle (matrix RC time + PCF8575 propagation)
            delayMicroseconds(200);
            
            // Read column states
            uint16_t pins = 0xFFFF;
            if (!pcf_.read(pins)) {
                continue;
            }
            
            // Process each column
            for (uint8_t c = 0; c < 8; c++)
            {
                bool down = ((pins >> cfg_.cols[c]) & 1) == 0;
                int btn = rowToBtn(r, c);
                if (btn < 0)
                    continue;
                
                bool last = lastDown_[r][c];
                if (down == last) {
                    continue; // No state change
                }
                    
                // State changed - check debounce
                uint32_t &deb = (r == cfg_.rowCtl) ? debCtlUntil_[c] : debUntil_[btn];
                if ((int32_t)(now - deb) < 0) {
                    continue; // Still in debounce window
                }
                
                // Valid state change - update and set new debounce window
                lastDown_[r][c] = down;
                deb = now + cfg_.debounce_us;
                
                // Debug output
                if (r != cfg_.rowCtl) {
                    Serial.printf("KB: btn=%d %s (mode=%d)\n", btn, down ? "DN" : "UP", (int)mode_);
                }
                
                if (r == cfg_.rowCtl)
                {
                    onControl(c, down);
                    continue;
                }
                
                // Musical/Step keys - delegate to current mode
                if (currentMode_)
                {
                    // Determine context based on mode
                    void *context = nullptr;
                    if (mode_ == Mode::Performance)
                    {
                        context = lastPitchOpt;
                    }
                    else if (mode_ == Mode::Cursor)
                    {
                        context = pattern_;
                    }

                    if (down)
                    {
                        currentMode_->onButtonDown(btn, midi, ch, context);
                    }
                    else
                    {
                        currentMode_->onButtonUp(btn, midi, ch, context);
                    }
                }
            }
            
            // Restore all pins high after each row to prevent ghosting
            pcf_.write(0xFFFF);
        }
        
        // Update mode (for gesture detection, etc.)
        if (currentMode_)
        {
            void *context = (mode_ == Mode::Cursor) ? pattern_ : nullptr;
            currentMode_->update(now, context);
        }
    }

private:
    PCF8575 pcf_;
    Config cfg_;
    uint32_t lastScanUs_{0};

    // Mode management
    Mode mode_{Mode::Performance};
    IMatrixKBMode *currentMode_{nullptr};
    PerformanceKBMode perfMode_;
    CursorMode cursorMode_;
    Pattern *pattern_{nullptr}; // Context for cursor mode

    // Hardware state
    bool lastDown_[3][8] = {};       // last read state per row/col
    uint32_t debUntil_[16] = {};     // per-button debounce time (musical buttons)
    uint32_t debCtlUntil_[8] = {};   // debounce for control row buttons

    // External systems
    RunLoop *rl_{nullptr};
    RecordEngine *rec_{nullptr};
    Transport *tx_{nullptr};
    class ViewManager *vm_{nullptr};

    bool driveRow(uint8_t r)
    {
        uint16_t v = 0xFFFF;
        v &= ~(1u << cfg_.rows[r]); // Drive active row low
        return pcf_.write(v);
    }

    int rowToBtn(uint8_t r, uint8_t c) const
    {
        if (r == cfg_.rowTop)
            return c; // 0..7  (top: digits)
        if (r == cfg_.rowBot)
            return 8 + c; // 8..15 (bottom: letters)
        if (r == cfg_.rowCtl)
            return 16 + c; // control buttons (optional)
        return -1;
    }

    void onControl(uint8_t c, bool down);
};