#include "generative_view.hpp"

void GenerativeView::begin(uint8_t midiChannel)
{
    midiChannel_ = midiChannel;
    
    MatrixKB::Config config;
    config.address = cfg::PCF_ADDRESS;
    mkb_.begin(config, 0, 4, 100);
    
    // Initialize generator manager
    generatorManager_.begin();
    
    Serial.println("GenerativeView initialized");
}

void GenerativeView::attach(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport, class ViewManager* viewManager)
{
    runLoop_ = runLoop;
    recordEngine_ = recordEngine;
    transport_ = transport;
    mkb_.attach(runLoop, recordEngine, transport);
    if (viewManager != nullptr) {
        mkb_.attachViewManager(viewManager);
    }
}

void GenerativeView::draw(Pattern& pattern, Viewport& viewport, OledRenderer& oled, 
                         MidiIO& midi, uint32_t now, uint32_t playTick)
{
    (void)midi;
    
    // Create HUD string for generative view
    char hud[64];
    drawHUD(hud, sizeof(hud));
    
    // Use the piano roll for pattern visualization
    PianoRoll::Options options = {};
    // Highlight the base note from current generator
    Generator* gen = generatorManager_.getCurrentGenerator();
    if (gen) {
        float baseNote;
        if (gen->getParameter("base_note", baseNote)) {
            options.highlightPitch = (uint8_t)baseNote;
        }
    }
    
    oled.rollSetOptions(options);
    oled.drawFrame(pattern, viewport, now, playTick, hud);
}

void GenerativeView::poll(MidiIO& midi)
{
    int lastPitch = -1;
    mkb_.poll(midi, midiChannel_, &lastPitch);
    
    // TODO: In the future, we can use the matrix keyboard for:
    // - Triggering generation
    // - Adjusting parameters 
    // - Switching between generators
    // For now, we rely on serial commands for parameter control
}

void GenerativeView::onActivate()
{
    Serial.println("=== GenerativeView Activated ===");
    Serial.println("Serial Commands:");
    Serial.println("  'g' - Generate pattern");
    Serial.println("  'n' - Next generator");
    Serial.println("  'b' - Previous generator");
    Serial.println("  'l' - List generators");
    Serial.println("  'i' - Generator info & parameters");
    Serial.println("  'r' - Reset to defaults");
    Serial.println("  'S<param> <value>' - Set parameter");
    Serial.println("    Examples: Sdensity 80, Slength 16");
    Serial.println("Matrix KB:");
    Serial.println("  Buttons 3/4/6 - Switch views");
    Serial.println("  Button 1 - Play/Pause");
    Serial.println("  Button 2 - Stop");
    Serial.println("================================");
    
    generatorManager_.printCurrentGenerator();
    lastGenTime_ = micros();
}

void GenerativeView::onDeactivate()
{
    Serial.println("GenerativeView deactivated");
}

void GenerativeView::triggerGeneration(Pattern& pattern)
{
    isGenerating_ = true;
    lastGenTime_ = micros();
    
    generatorManager_.generatePattern(pattern);
    
    isGenerating_ = false;
}

void GenerativeView::switchToNextGenerator()
{
    generatorManager_.switchToNextGenerator();
}

void GenerativeView::switchToPreviousGenerator()
{
    generatorManager_.switchToPreviousGenerator();
}

void GenerativeView::resetToDefaults()
{
    generatorManager_.resetCurrentGeneratorToDefaults();
}

void GenerativeView::drawHUD(char* buffer, size_t bufferSize) const
{
    Generator* gen = generatorManager_.getCurrentGenerator();
    if (gen) {
        // Get some key parameters for display
        float density = 0, length = 0;
        gen->getParameter("density", density);
        gen->getParameter("length", length);
        
        snprintf(buffer, bufferSize, "GEN:%s D:%.0f L:%.0f %s", 
                 gen->getShortName(),
                 density,
                 length,
                 isGenerating_ ? "..." : "RDY");
    } else {
        snprintf(buffer, bufferSize, "GEN: NO GENERATOR");
    }
}