#include "config.h"
#include "ignition.h"
#include "motion.h"
#include "sleep.h"
#include "sms.h"

#include <Arduino.h>
#include <esp_sleep.h>

RTC_DATA_ATTR bool guardArmed = false;
RTC_DATA_ATTR bool manualDisarm = false;
RTC_DATA_ATTR uint32_t graceRemainingSec = ARMING_GRACE_SECONDS;
RTC_DATA_ATTR uint32_t muteRemainingSec = 0;
RTC_DATA_ATTR uint32_t alertCooldownSec = 0;
RTC_DATA_ATTR uint32_t bootCount = 0;

namespace {

bool isOwner(const String& from) {
    String normalized = from;
    normalized.trim();
    return normalized == ALERT_PHONE;
}

String upperTrim(String s) {
    s.trim();
    s.toUpperCase();
    return s;
}

void sendStatusReply(const String& to, esp_sleep_wakeup_cause_t wakeCause) {
    String msg = "STATUS ";
    msg += guardArmed ? "ARMED" : "DISARMED";
    msg += ", IGN=";
    msg += ignitionOn() ? "ON" : "OFF";
    msg += ", WAKE=";
    msg += String(static_cast<int>(wakeCause));
    msg += ", MUTE=";
    msg += String(muteRemainingSec);
    msg += "s, COOL=";
    msg += String(alertCooldownSec);
    msg += "s, ";
    msg += modemGetSignalAndPowerStatus();
    (void)smsSend(to.c_str(), msg);
}

void processCommand(const SmsCommand& cmd, esp_sleep_wakeup_cause_t wakeCause) {
    if (!isOwner(cmd.from)) {
        smsDeleteIndex(cmd.index);
        return;
    }

    String body = upperTrim(cmd.body);

    if (body.startsWith("MUTE")) {
        int spacePos = body.indexOf(' ');
        if (spacePos < 0) {
            (void)smsSend(ALERT_PHONE, "MUTE ERROR");
        } else {
            String value = body.substring(spacePos + 1);
            value.trim();
            bool allDigits = value.length() > 0;
            for (size_t ci = 0; ci < value.length(); ++ci) {
                if (!isdigit(static_cast<unsigned char>(value[ci]))) {
                    allDigits = false;
                    break;
                }
            }
            if (!allDigits) {
                (void)smsSend(ALERT_PHONE, "MUTE ERROR");
                smsDeleteIndex(cmd.index);
                return;
            }
            int minutes = value.toInt();
            if (minutes < 0 || minutes > MAX_MUTE_MINUTES) {
                (void)smsSend(ALERT_PHONE, "MUTE RANGE 0-1440");
            } else {
                muteRemainingSec = static_cast<uint32_t>(minutes) * 60U;
                if (minutes == 0) {
                    (void)smsSend(ALERT_PHONE, "MUTE OFF");
                } else {
                    (void)smsSend(ALERT_PHONE, "MUTE ON " + String(minutes) + " MIN");
                }
            }
        }
    } else if (body == "STATUS") {
        sendStatusReply(cmd.from, wakeCause);
    } else if (body == "ARM ON") {
        guardArmed = true;
        manualDisarm = false;
        graceRemainingSec = 0;
        (void)smsSend(ALERT_PHONE, "ARMED");
    } else if (body == "ARM OFF") {
        guardArmed = false;
        manualDisarm = true;
        graceRemainingSec = 0;
        (void)smsSend(ALERT_PHONE, "DISARMED");
    } else if (body == "PING") {
        (void)smsSend(ALERT_PHONE, "PONG");
    } else {
        (void)smsSend(ALERT_PHONE, "CMD? MUTE N | STATUS | ARM ON | ARM OFF | PING");
    }

    smsDeleteIndex(cmd.index);
}

void updateGuardFromIgnition() {
    if (ignitionOn()) {
        guardArmed = false;
        manualDisarm = false;
        graceRemainingSec = ARMING_GRACE_SECONDS;
        return;
    }

    if (manualDisarm) {
        guardArmed = false;
        return;
    }

    if (guardArmed) {
        return;
    }

    if (graceRemainingSec > PARK_TIMER_WAKE_SECONDS) {
        graceRemainingSec -= PARK_TIMER_WAKE_SECONDS;
        return;
    }

    graceRemainingSec = 0;
    guardArmed = true;
}

void decayTimers(uint32_t stepSeconds) {
    if (muteRemainingSec > stepSeconds) {
        muteRemainingSec -= stepSeconds;
    } else {
        muteRemainingSec = 0;
    }

    if (alertCooldownSec > stepSeconds) {
        alertCooldownSec -= stepSeconds;
    } else {
        alertCooldownSec = 0;
    }
}

void maybeSendMotionAlert(esp_sleep_wakeup_cause_t wakeCause) {
    if (wakeCause != ESP_SLEEP_WAKEUP_EXT0) {
        return;
    }
    if (!guardArmed || muteRemainingSec > 0 || alertCooldownSec > 0) {
        return;
    }

    const String msg = "ALERT motion detected (shake wake)";
    if (smsSend(ALERT_PHONE, msg)) {
        alertCooldownSec = ALERT_COOLDOWN_SECONDS;
    }
}

} // namespace

void setup() {
    ++bootCount;

    Serial.begin(115200);
    delay(50);

    esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    Serial.printf("[BOOT] count=%u wake=%d\n", static_cast<unsigned>(bootCount), static_cast<int>(wakeCause));

    ignitionInit();

    if (!motionInitLowPowerInterrupt()) {
        Serial.println("[BOOT] motion init failed");
    }

    if (wakeCause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        if (!modemInit()) {
            Serial.println("[BOOT] modem init failed");
        }
    } else {
        modemPreparePinsForWake();
        if (!modemWake()) {
            Serial.println("[BOOT] modem wake failed");
        }
    }
    updateGuardFromIgnition();

    SmsCommand commands[6];
    size_t cmdCount = smsPollUnread(commands, 6);
    for (size_t i = 0; i < cmdCount; ++i) {
        processCommand(commands[i], wakeCause);
    }

    maybeSendMotionAlert(wakeCause);

    decayTimers(PARK_TIMER_WAKE_SECONDS);

    motionClearInterrupt();
    modemEnterSleep();

    if (!ignitionOn()) {
        boardDeepSleep(PARK_TIMER_WAKE_SECONDS);
    }
}

void loop() {
    static uint32_t lastTick = 0;
    uint32_t now = millis();
    if (now - lastTick < 1000) {
        delay(10);
        return;
    }
    lastTick = now;

    if (!modemEnsureNetwork()) {
        return;
    }

    SmsCommand commands[4];
    size_t cmdCount = smsPollUnread(commands, 4);
    for (size_t i = 0; i < cmdCount; ++i) {
        processCommand(commands[i], ESP_SLEEP_WAKEUP_UNDEFINED);
    }

    if (!ignitionOn()) {
        modemEnterSleep();
        boardDeepSleep(PARK_TIMER_WAKE_SECONDS);
    }
}
