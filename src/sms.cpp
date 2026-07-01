#include "sms.h"
#include "config.h"
#include <Arduino.h>

#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>

HardwareSerial modemSerial(1);
TinyGsm        modem(modemSerial);

bool smsInit() {
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

    // Power on SIM800L
    pinMode(MODEM_POWER_ON, OUTPUT);
    digitalWrite(MODEM_POWER_ON, HIGH);

    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, LOW);

    delay(3000);

    if (!modem.restart()) {
        Serial.println("[SMS] Modem restart failed");
        return false;
    }

    String modemInfo = modem.getModemInfo();
    Serial.print("[SMS] Modem: ");
    Serial.println(modemInfo);

    if (!modem.waitForNetwork(30000)) {
        Serial.println("[SMS] Network not available");
        return false;
    }

    Serial.println("[SMS] Network OK");
    return true;
}

void smsModemSleep() {
    // SIM800L sleep mode — stays registered on network, draws ~1mA instead of ~20mA
    modem.sendAT("+CSCLK=2");
    modem.waitResponse();
    Serial.println("[SMS] Modem sleeping");
}

void smsModemWake() {
    // Toggle DTR low to wake — on V1.3 we pulse PWRKEY briefly instead
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(500);
    modem.sendAT("+CSCLK=0");
    modem.waitResponse();
    if (!modem.isNetworkConnected()) {
        modem.waitForNetwork(10000);
    }
    Serial.println("[SMS] Modem awake");
}

bool smsSend(const char* number, const char* message) {
    if (!modem.isNetworkConnected()) {
        Serial.println("[SMS] Not connected, retrying network...");
        if (!modem.waitForNetwork(15000)) {
            return false;
        }
    }

    bool ok = modem.sendSMS(number, message);
    if (ok) {
        Serial.print("[SMS] Sent to ");
        Serial.println(number);
    } else {
        Serial.println("[SMS] Send failed");
    }
    return ok;
}
