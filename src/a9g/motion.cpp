#include "motion.h"
#include <Arduino.h>

bool motionInit() {
    Serial.println("[MOTION] A9G stub init");
    return true;
}

bool motionDetected() {
    Serial.println("[MOTION] A9G stub: motion detection unavailable");
    return false;
}
