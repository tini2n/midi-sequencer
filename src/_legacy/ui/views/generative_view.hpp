#pragma once
#include <Arduino.h>

#include "ui/views/base_view.hpp"
#include "io/matrix_kb.hpp"
#include "engine/generator_manager.hpp"
#include "io/encoder_manager.hpp"
#include "io/cursor_mode.hpp"
#include "config.hpp"

#include "engine/generator_manager.hpp"
#include "ui/views/base_view.hpp"

#include "io/matrix_kb.hpp"
#include "io/encoder_manager.hpp"

/**
 * Generative view for algorithmic pattern creation.
 * Pure UI/rendering class that delegates all generation logic to GeneratorManager.
 * Responsible only for displaying generator status and handling input.
 */
class GenerativeView : public IView, public IEncoderHandler
{
public:
    // IView interface implementation
    void begin(uint8_t midiChannel) override;
    void attach(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport, class ViewManager* viewManager = nullptr) override;
    
    void setPattern(Pattern* pattern) { pattern_ = pattern; }
    
    void draw(Pattern& pattern, Viewport& viewport, OledRenderer& oled, 
              MidiIO& midi, uint32_t now, uint32_t playTick) override;
    void poll(MidiIO& midi) override;
    void onActivate() override;
    void onDeactivate() override;
    const char* getName() const override { return "Generative"; }
    
    IEncoderHandler* getEncoderHandler() override { return this; }

    // Generative-specific methods
    void triggerGeneration(Pattern& pattern);
    void switchToNextGenerator();
    void switchToPreviousGenerator();
    void resetToDefaults();
    
    GeneratorManager& getGeneratorManager() { return generatorManager_; }
    
    // IEncoderHandler interface
    void onEncoderRotation(const EncoderRotationEvent& event) override;
    void onEncoderButton(const EncoderButtonEvent& event) override;

private:
    MatrixKB* mkb_{nullptr};
    Pattern* pattern_{nullptr};
    RunLoop* runLoop_{nullptr};
    RecordEngine* recordEngine_{nullptr};
    Transport* transport_{nullptr};
    GeneratorManager generatorManager_;
    uint8_t midiChannel_{1};
    CursorMode cursorMode_;
    
    // Cached edit pitch (updated when encoder changes, not in draw)
    uint8_t cachedEditPitch_{60};
    
    // UI state
    bool isGenerating_{false};
    uint32_t lastGenTime_{0};
    
    // UI helpers
    void drawHUD(char* buffer, size_t bufferSize) const;

public:
    void setMatrixKB(MatrixKB* mkb) { mkb_ = mkb; }
};