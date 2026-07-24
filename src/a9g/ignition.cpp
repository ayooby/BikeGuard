#include "ignition.h"
#include <Arduino.h>

void ignitionInit() {
    Serial.println("[IGNITION] A9G stub init");
}

bool ignitionOn() {
    Serial.println("[IGNITION] A9G stub: ignition state unavailable");
    return false;
}
