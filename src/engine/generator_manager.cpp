#include "generator_manager.hpp"
#include "euclidean_generator.hpp"

GeneratorManager::GeneratorManager()
{
}

void GeneratorManager::begin()
{
    // Register available generators
    registerGenerator(std::make_unique<EuclideanGenerator>());
    
    // TODO: Add more generators here as they're implemented
    // registerGenerator(std::make_unique<CellularGenerator>());
    // registerGenerator(std::make_unique<MarkovGenerator>());
    // registerGenerator(std::make_unique<ProbabilityGenerator>());
    
    Serial.printf("GeneratorManager: Registered %d generators\n", generators_.size());
    printAvailableGenerators();
}

void GeneratorManager::registerGenerator(std::unique_ptr<Generator> generator)
{
    if (generator) {
        Serial.printf("Registered generator: %s\n", generator->getName());
        generators_.push_back(std::move(generator));
    }
}

bool GeneratorManager::switchToGenerator(size_t index)
{
    if (!isValidIndex(index)) {
        Serial.printf("GeneratorManager: Invalid generator index %d\n", index);
        return false;
    }
    
    currentIndex_ = index;
    Serial.printf("Switched to generator: %s\n", getCurrentGenerator()->getName());
    return true;
}

void GeneratorManager::switchToNextGenerator()
{
    if (generators_.empty()) return;
    
    size_t nextIndex = (currentIndex_ + 1) % generators_.size();
    switchToGenerator(nextIndex);
}

void GeneratorManager::switchToPreviousGenerator()
{
    if (generators_.empty()) return;
    
    size_t prevIndex = (currentIndex_ + generators_.size() - 1) % generators_.size();
    switchToGenerator(prevIndex);
}

Generator* GeneratorManager::getCurrentGenerator() const
{
    if (isValidIndex(currentIndex_)) {
        return generators_[currentIndex_].get();
    }
    return nullptr;
}

void GeneratorManager::generatePattern(Pattern& pattern)
{
    Generator* gen = getCurrentGenerator();
    if (gen) {
        Serial.printf("Generating pattern with %s...\n", gen->getName());
        gen->generate(pattern);
    } else {
        Serial.println("GeneratorManager: No generator available");
    }
}

bool GeneratorManager::setParameter(const char* paramName, float value)
{
    Generator* gen = getCurrentGenerator();
    if (gen) {
        bool success = gen->setParameter(paramName, value);
        if (success) {
            Serial.printf("Set %s = %.2f\n", paramName, value);
        } else {
            Serial.printf("Parameter '%s' not found\n", paramName);
        }
        return success;
    }
    return false;
}

bool GeneratorManager::getParameter(const char* paramName, float& outValue) const
{
    Generator* gen = getCurrentGenerator();
    if (gen) {
        return gen->getParameter(paramName, outValue);
    }
    return false;
}

std::vector<const char*> GeneratorManager::getCurrentParameterNames() const
{
    Generator* gen = getCurrentGenerator();
    if (gen) {
        return gen->getParameterNames();
    }
    return {};
}

void GeneratorManager::printCurrentGenerator() const
{
    Generator* gen = getCurrentGenerator();
    if (gen) {
        Serial.printf("Current Generator: %s (%s)\n", gen->getName(), gen->getShortName());
        gen->printParameters();
    } else {
        Serial.println("No generator selected");
    }
}

void GeneratorManager::printAvailableGenerators() const
{
    Serial.println("=== Available Generators ===");
    for (size_t i = 0; i < generators_.size(); i++) {
        const char* current = (i == currentIndex_) ? " *" : "  ";
        Serial.printf("%s[%d] %s (%s)\n", current, i, 
                     generators_[i]->getName(), 
                     generators_[i]->getShortName());
    }
    Serial.println("============================");
}

void GeneratorManager::resetCurrentGeneratorToDefaults()
{
    Generator* gen = getCurrentGenerator();
    if (gen) {
        gen->resetToDefaults();
        Serial.printf("Reset %s to defaults\n", gen->getName());
    }
}

bool GeneratorManager::isValidIndex(size_t index) const
{
    return index < generators_.size();
}