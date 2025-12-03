#include "view_manager.hpp"
#include "performance_view.hpp"
#include "generative_view.hpp"

void ViewManager::registerView(ViewType viewType, IView* view)
{
    if (view == nullptr) {
        Serial.println("Warning: Attempted to register null view");
        return;
    }
    
    size_t index = static_cast<size_t>(viewType);
    if (index >= views_.size()) {
        Serial.println("Warning: Invalid view type in registerView");
        return;
    }
    
    views_[index] = view;
    
    // If this is the first view registered, make it the current view
    if (currentView_ == nullptr && viewType == currentViewType_) {
        currentView_ = view;
    }
}

void ViewManager::initialize(RunLoop* runLoop, RecordEngine* recordEngine, Transport* transport,
                            Pattern* pattern, uint8_t midiChannel,
                            const EncoderManager::PinConfig encoderConfigs[EncoderManager::NUM_ENCODERS],
                            uint32_t encoderDebounceUs)
{
    // Initialize all views
    for (auto* view : views_) {
        if (view != nullptr) {
            view->begin(midiChannel);
            view->attach(runLoop, recordEngine, transport, this);
            view->setPattern(pattern);
        }
    }
    
    // Initialize encoders
    encoderMgr_.begin(encoderConfigs, encoderDebounceUs);
    encodersInitialized_ = true;
    
    Serial.println("ViewManager initialized: views + encoders ready");
}

bool ViewManager::switchToView(ViewType viewType)
{
    size_t index = static_cast<size_t>(viewType);
    if (index >= views_.size() || views_[index] == nullptr) {
        Serial.printf("Warning: Cannot switch to unregistered view type %d\n", static_cast<int>(viewType));
        return false;
    }
    
    if (viewType == currentViewType_) {
        return true; // Already on this view
    }
    
    // Deactivate current view
    if (currentView_ != nullptr) {
        currentView_->onDeactivate();
    }
    
    // Switch to new view
    currentViewType_ = viewType;
    currentView_ = views_[index];
    
    // Activate new view
    if (currentView_ != nullptr) {
        currentView_->onActivate();
        
        // Set encoder handler for the newly activated view if encoders are initialized
        // Both PerformanceView and GenerativeView implement IEncoderHandler
        if (encodersInitialized_) {
            IEncoderHandler* handler = nullptr;
            
            // Type safety: ViewType enum guarantees currentView_ matches expected type
            if (currentViewType_ == ViewType::Performance) {
                handler = static_cast<IEncoderHandler*>(static_cast<PerformanceView*>(currentView_));
            } else if (currentViewType_ == ViewType::Generative) {
                handler = static_cast<IEncoderHandler*>(static_cast<GenerativeView*>(currentView_));
            }
            
            encoderMgr_.setHandler(handler);
        }
    }
    
    Serial.printf("Switched to view: %s\n", getCurrentViewName());
    return true;
}

void ViewManager::switchToNextView()
{
    // Switch between Performance and Generative views only
    ViewType nextViewType;
    if (currentViewType_ == ViewType::Performance) {
        nextViewType = ViewType::Generative;
    } else {
        nextViewType = ViewType::Performance;
    }
    
    switchToView(nextViewType);
}

void ViewManager::switchToPreviousView()
{
    // Same logic as next view since we only have 2 views
    switchToNextView();
}

void ViewManager::draw(Pattern& pattern, Viewport& viewport, OledRenderer& oled, 
                      MidiIO& midi, uint32_t now, uint32_t playTick)
{
    if (currentView_ != nullptr) {
        currentView_->draw(pattern, viewport, oled, midi, now, playTick);
    }
}

void ViewManager::pollKB(MidiIO& midi)
{
    if (currentView_ != nullptr) {
        currentView_->poll(midi);
    }
}

void ViewManager::pollEncoders()
{
    // Handler was already set in activateView(), just poll
    encoderMgr_.poll();
}

const char* ViewManager::getCurrentViewName() const
{
    if (currentView_ != nullptr) {
        return currentView_->getName();
    }
    return "None";
}