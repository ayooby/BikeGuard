#include "motion.h"
#include "config.h"

#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t MPU_REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t MPU_REG_INT_PIN_CFG = 0x37;
constexpr uint8_t MPU_REG_INT_ENABLE = 0x38;
constexpr uint8_t MPU_REG_INT_STATUS = 0x3A;
constexpr uint8_t MPU_REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t MPU_REG_MOT_THR = 0x1F;
constexpr uint8_t MPU_REG_MOT_DUR = 0x20;
constexpr uint8_t MPU_REG_MOT_DETECT_CTRL = 0x69;
constexpr uint8_t MPU_REG_WHO_AM_I = 0x75;

bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint8_t* value) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom(MPU_ADDR, 1) != 1) {
        return false;
    }
    *value = Wire.read();
    return true;
}

} // namespace

bool motionInitLowPowerInterrupt() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    pinMode(MPU_INT_PIN, INPUT);

    uint8_t whoami = 0;
    if (!readReg(MPU_REG_WHO_AM_I, &whoami)) {
        Serial.println("[MOTION] failed to read WHO_AM_I");
        return false;
    }
    if (whoami != 0x68) {
        Serial.printf("[MOTION] unexpected WHO_AM_I=0x%02X\n", whoami);
        return false;
    }

    // Wake MPU and configure motion interrupt.
    if (!writeReg(MPU_REG_PWR_MGMT_1, 0x00)) {
        return false;
    }
    delay(50);

    // +-2g range for best sensitivity.
    if (!writeReg(MPU_REG_ACCEL_CONFIG, 0x00)) {
        return false;
    }

    // Motion threshold and duration are conservative defaults for vehicle shake.
    if (!writeReg(MPU_REG_MOT_THR, 20)) {
        return false;
    }
    if (!writeReg(MPU_REG_MOT_DUR, 40)) {
        return false;
    }

    // Use decrement counters to avoid triggering on brief spikes.
    if (!writeReg(MPU_REG_MOT_DETECT_CTRL, 0x15)) {
        return false;
    }

    // Latched INT, active high.
    if (!writeReg(MPU_REG_INT_PIN_CFG, 0x20)) {
        return false;
    }

    // Enable Motion interrupt.
    if (!writeReg(MPU_REG_INT_ENABLE, 0x40)) {
        return false;
    }

    motionClearInterrupt();
    Serial.println("[MOTION] MPU6050 motion interrupt ready");
    return true;
}

void motionClearInterrupt() {
    uint8_t status = 0;
    (void)readReg(MPU_REG_INT_STATUS, &status);
}
