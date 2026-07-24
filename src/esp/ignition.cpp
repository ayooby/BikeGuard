#include "ignition.h"
#include "config.h"
#include <Arduino.h>

void ignitionInit() {
    // GPIO 35 is input-only on ESP32, no pull-up/down available
    // The voltage divider handles the signal level — no internal pull needed
    pinMode(IGNITION_PIN, INPUT);
    Serial.println("[IGNITION] Pin configured");
}

bool ignitionOn() {
#ifdef BIKEGUARD_FORCE_IGNITION_OFF
    return false;
#else
    // Voltage divider gives ~2.1V when 12V present (HIGH)
    // Gives 0V when ignition off (LOW)
    return digitalRead(IGNITION_PIN) == HIGH;
#endif
}
