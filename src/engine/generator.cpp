#include "generator.hpp"

void Generator::printParameters() const
{
    Serial.printf("=== %s Generator Parameters ===\n", getName());
    
    const auto& params = getParameters();
    for (const auto& pair : params) {
        const auto& param = pair.second;
        Serial.printf("%s: %.2f (%.2f-%.2f) - %s\n", 
                     param.name, 
                     param.value, 
                     param.minValue, 
                     param.maxValue,
                     param.description);
    }
    Serial.println("=====================================");
}