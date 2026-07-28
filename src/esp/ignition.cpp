#include "ignition.h"
#include "config.h"
#include <Arduino.h>

void ignitionInit() {
    // GPIO35 is input-only on ESP32. Ignition line must use an external divider.
    pinMode(IGNITION_PIN, INPUT);
    Serial.println("[IGNITION] configured on GPIO35");
}

bool ignitionOn() {
    return digitalRead(IGNITION_PIN) == HIGH;
}
