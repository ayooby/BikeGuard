#include <Arduino.h>
#include "config.h"
#include "ignition.h"
#include "motion.h"
#include "sms.h"
#include "sleep.h"

typedef enum {
    STATE_IDLE,    // bike is on — not monitoring
    STATE_ARMED,   // bike is off — watching for movement
    STATE_TIMING,  // movement detected — counting down before alert
    STATE_ALERT,   // SMS sent — waiting for ignition to come back on
} AlarmState;

static AlarmState state = STATE_IDLE;
static unsigned long motionStartTime = 0;
static bool modemAsleep = false;

static void enterArmed() {
    Serial.println("[STATE] → ARMED");
    smsModemSleep();
    modemAsleep = true;
    state = STATE_ARMED;
}

static void exitArmed() {
    if (modemAsleep) {
        smsModemWake();
        modemAsleep = false;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("[BOOT] Theft controller starting...");

    ignitionInit();
    if (!motionInit()) {
        Serial.println("[BOOT] FATAL: MPU6050 failed");
        while (true) delay(1000);
    }
    if (!smsInit()) {
        Serial.println("[BOOT] FATAL: SIM800L failed");
        while (true) delay(1000);
    }

    Serial.println("[BOOT] All systems OK");
}

void loop() {
    bool bikeOn = ignitionOn();
    bool moving  = motionDetected();
    unsigned long now = millis();

    switch (state) {

        case STATE_IDLE:
            if (!bikeOn) {
                enterArmed();
            }
            break;

        case STATE_ARMED:
            if (bikeOn) {
                exitArmed();
                Serial.println("[STATE] → IDLE (ignition on)");
                state = STATE_IDLE;
                break;
            }
            if (moving) {
                exitArmed();
                Serial.println("[STATE] → TIMING (movement detected)");
                motionStartTime = now;
                state = STATE_TIMING;
                break;
            }
            // No movement — light sleep to save power, wake and re-check
            Serial.flush();
            boardSleep();
            break;

        case STATE_TIMING:
            if (bikeOn) {
                Serial.println("[STATE] → IDLE (ignition on during timing)");
                state = STATE_IDLE;
                break;
            }
            if (!moving) {
                Serial.println("[STATE] → ARMED (movement stopped)");
                enterArmed();
                break;
            }
            if (now - motionStartTime >= MOTION_TIMEOUT_MS) {
                Serial.println("[STATE] → ALERT");
                smsSend(ALERT_PHONE, "ALERT: Bike is moving while ignition is OFF!");
                state = STATE_ALERT;
            }
            break;

        case STATE_ALERT:
            if (bikeOn) {
                Serial.println("[STATE] → IDLE (ignition on, alert cleared)");
                state = STATE_IDLE;
            }
            // Stay in ALERT until owner turns key — don't spam SMS
            break;
    }

    delay(50);
}
