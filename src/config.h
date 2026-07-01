#pragma once

// ── SIM800L (reserved by T-Call V1.3 hardware) ──────────────────────────────
#define MODEM_TX        26
#define MODEM_RX        27
#define MODEM_PWRKEY    4
#define MODEM_RST       5
#define MODEM_POWER_ON  23
#define MODEM_BAUD      9600

// ── IP5306 power management I2C (reserved by T-Call V1.3 hardware) ──────────
#define IP5306_SDA      21
#define IP5306_SCL      22

// ── MPU6050 I2C (remapped — ESP32 allows any pin for I2C) ───────────────────
#define MPU_SDA         14
#define MPU_SCL         15

// ── Ignition sense ───────────────────────────────────────────────────────────
// 12V switched line → 10kΩ / 47kΩ voltage divider → ~2.4V at GPIO
// GPIO 35 is input-only, no internal pull-up/down — safe for this use
#define IGNITION_PIN    35

// ── SMS config ───────────────────────────────────────────────────────────────
#define ALERT_PHONE     "+4500000000"   // replace with your number

// ── Thresholds ───────────────────────────────────────────────────────────────
// Acceleration magnitude delta that counts as movement (in m/s²)
#define MOTION_THRESHOLD    1.5f

// How long motion must be detected before alert fires (ms)
// Covers sidestand, loading bike, brief bumps
#define MOTION_TIMEOUT_MS   10000

// How often we sample the MPU6050 (ms)
#define MOTION_SAMPLE_MS    200
