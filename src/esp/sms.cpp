#include "sms.h"
#include "config.h"

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <Wire.h>

namespace {

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

bool setBoostKeepOn() {
    // Same IP5306 write sequence as LilyGO official examples.
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(IP5306_REG_SYS_CTL0);
    Wire.write(0x37);
    return Wire.endTransmission() == 0;
}

void setupModemPins(bool pulsePwrKey) {
    pinMode(MODEM_RST_PIN, OUTPUT);
    digitalWrite(MODEM_RST_PIN, HIGH);

    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    pinMode(MODEM_POWER_ON_PIN, OUTPUT);
    pinMode(MODEM_DTR_PIN, OUTPUT);

    digitalWrite(MODEM_POWER_ON_PIN, HIGH);
    digitalWrite(MODEM_DTR_PIN, LOW);

    if (pulsePwrKey) {
        // Verified from LilyGO utilities.h: HIGH -> LOW(1s) -> HIGH
        digitalWrite(MODEM_PWRKEY_PIN, HIGH);
        delay(100);
        digitalWrite(MODEM_PWRKEY_PIN, LOW);
        delay(1000);
        digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    } else {
        digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    }
}

bool atSend(const String& cmd, String* response, uint32_t timeoutMs = 3000) {
    while (modemSerial.available()) {
        modemSerial.read();
    }

    modemSerial.print(cmd);
    modemSerial.print("\r\n");

    String out;
    const uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        while (modemSerial.available()) {
            char c = static_cast<char>(modemSerial.read());
            out += c;
        }
        if (out.indexOf("\r\nOK\r\n") >= 0) {
            if (response != nullptr) {
                *response = out;
            }
            return true;
        }
        if (out.indexOf("\r\nERROR\r\n") >= 0) {
            if (response != nullptr) {
                *response = out;
            }
            return false;
        }
        delay(5);
    }

    if (response != nullptr) {
        *response = out;
    }
    return false;
}

String trimLine(String s) {
    s.replace("\r", "");
    s.replace("\n", "");
    s.trim();
    return s;
}

bool parseCmglHeader(const String& line, int* indexOut, String* fromOut) {
    // Example: +CMGL: 1,"REC UNREAD","+4512345678","","26/07/28,12:34:56+08"
    const int colon = line.indexOf(':');
    if (colon < 0) {
        return false;
    }

    const int firstComma = line.indexOf(',', colon + 1);
    if (firstComma < 0) {
        return false;
    }

    String idxText = line.substring(colon + 1, firstComma);
    idxText.trim();
    *indexOut = idxText.toInt();

    // Skip past the status field ("REC UNREAD") to land on the open-quote of
    // the phone number field.
    int quotePos = line.indexOf('"', firstComma + 1);
    for (int i = 0; i < 2 && quotePos >= 0; ++i) {
        quotePos = line.indexOf('"', quotePos + 1);
    }
    if (quotePos < 0) {
        return false;
    }

    // quotePos is now the open-quote of the phone number field.
    int phoneEnd = line.indexOf('"', quotePos + 1);
    if (phoneEnd < 0) {
        return false;
    }

    *fromOut = line.substring(quotePos + 1, phoneEnd);
    fromOut->trim();
    return *indexOut > 0 && !fromOut->isEmpty();
}

} // namespace

bool modemInit() {
    if (!setBoostKeepOn()) {
        Serial.println("[PMU] IP5306 keep-on write failed");
    }

    setupModemPins(true);
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    delay(6000);

    if (!modem.restart()) {
        Serial.println("[MODEM] restart failed");
        return false;
    }

    if (!modem.waitForNetwork(NETWORK_WAIT_MS)) {
        Serial.println("[MODEM] network not ready");
        return false;
    }

    if (!atSend("AT+CMGF=1", nullptr, 3000)) {
        Serial.println("[SMS] CMGF=1 failed");
        return false;
    }

    // Keep SMS indications stored in memory and read by polling.
    if (!atSend("AT+CNMI=1,1,0,0,0", nullptr, 3000)) {
        Serial.println("[SMS] CNMI setup failed");
    }

    Serial.print("[MODEM] ");
    Serial.println(modem.getModemInfo());
    return true;
}

bool modemWake() {
    // After deep sleep the ESP32 cold-boots, so restore the control pins and
    // UART without replaying the power-key pulse used for cold power-on.
    if (!setBoostKeepOn()) {
        Serial.println("[PMU] IP5306 keep-on write failed");
    }

    setupModemPins(false);
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    delay(200);

    // DTR low wakes SIM800L from AT+CSCLK=2 sleep without a full restart.
    digitalWrite(MODEM_DTR_PIN, LOW);
    delay(50);

    modemExitSleep();

    if (!modem.waitForNetwork(NETWORK_WAIT_MS)) {
        Serial.println("[MODEM] network not ready after wake");
        return false;
    }

    Serial.println("[MODEM] wake ok");
    return true;
}

bool modemEnsureNetwork() {
    if (modem.isNetworkConnected()) {
        return true;
    }
    return modem.waitForNetwork(15000);
}

bool smsSend(const char* number, const String& message) {
    if (!modemEnsureNetwork()) {
        return false;
    }
    return modem.sendSMS(number, message);
}

size_t smsPollUnread(SmsCommand* outCommands, size_t maxCommands) {
    if (outCommands == nullptr || maxCommands == 0) {
        return 0;
    }

    String raw;
    if (!atSend("AT+CMGL=\"REC UNREAD\"", &raw, 5000)) {
        return 0;
    }

    size_t count = 0;
    int cursor = 0;
    while (count < maxCommands) {
        int headerPos = raw.indexOf("+CMGL:", cursor);
        if (headerPos < 0) {
            break;
        }

        int headerEnd = raw.indexOf('\n', headerPos);
        if (headerEnd < 0) {
            break;
        }

        String header = trimLine(raw.substring(headerPos, headerEnd + 1));

        int bodyStart = headerEnd + 1;
        while (bodyStart < static_cast<int>(raw.length()) && (raw[bodyStart] == '\r' || raw[bodyStart] == '\n')) {
            ++bodyStart;
        }

        int bodyEnd = raw.indexOf('\n', bodyStart);
        if (bodyEnd < 0) {
            break;
        }

        String body = trimLine(raw.substring(bodyStart, bodyEnd + 1));
        int idx = 0;
        String from;

        if (parseCmglHeader(header, &idx, &from) && !body.isEmpty()) {
            outCommands[count].index = idx;
            outCommands[count].from = from;
            outCommands[count].body = body;
            ++count;
        }

        cursor = bodyEnd + 1;
    }

    return count;
}

void smsDeleteIndex(int index) {
    if (index <= 0) {
        return;
    }
    String cmd = "AT+CMGD=" + String(index);
    (void)atSend(cmd, nullptr, 3000);
}

void modemEnterSleep() {
    digitalWrite(MODEM_DTR_PIN, HIGH);
    (void)atSend("AT+CSCLK=2", nullptr, 3000);
}

void modemExitSleep() {
    digitalWrite(MODEM_DTR_PIN, LOW);
    (void)atSend("AT+CSCLK=0", nullptr, 3000);
}

void modemPreparePinsForWake() {
    setupModemPins(false);
}

String modemGetSignalAndPowerStatus() {
    String csq;
    String cbc;
    (void)atSend("AT+CSQ", &csq, 3000);
    (void)atSend("AT+CBC", &cbc, 3000);

    String out = "CSQ=";
    const int csqPos = csq.indexOf("+CSQ:");
    if (csqPos >= 0) {
        int eol = csq.indexOf('\n', csqPos);
        if (eol < 0) {
            eol = csq.length();
        }
        out += trimLine(csq.substring(csqPos + 5, eol));
    } else {
        out += "NA";
    }

    out += ", CBC=";
    const int cbcPos = cbc.indexOf("+CBC:");
    if (cbcPos >= 0) {
        int eol = cbc.indexOf('\n', cbcPos);
        if (eol < 0) {
            eol = cbc.length();
        }
        out += trimLine(cbc.substring(cbcPos + 5, eol));
    } else {
        out += "NA";
    }

    return out;
}
