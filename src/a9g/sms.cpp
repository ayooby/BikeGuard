#include "sms.h"
#include <Arduino.h>

bool smsInit() {
    Serial.println("[SMS] A9G stub init");
    return true;
}

void smsModemSleep() {
    Serial.println("[SMS] A9G stub sleep");
}

void smsModemWake() {
    Serial.println("[SMS] A9G stub wake");
}

bool smsSend(const char* number, const char* message) {
    Serial.print("[SMS] A9G stub send to ");
    Serial.print(number);
    Serial.print(": ");
    Serial.println(message);
    return true;
}
