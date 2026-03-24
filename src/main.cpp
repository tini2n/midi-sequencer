#include <Arduino.h>
#include "app.hpp"

static App app;

void setup()
{
    app.setup();
}

void loop()
{
    app.update();
}