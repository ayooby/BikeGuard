#pragma once

// LilyGO T-Call SIM800L IP5306 pin mapping (verified from official examples).
static constexpr int MODEM_RST_PIN = 5;
static constexpr int MODEM_PWRKEY_PIN = 4;
static constexpr int MODEM_POWER_ON_PIN = 23;
static constexpr int MODEM_TX_PIN = 27;
static constexpr int MODEM_RX_PIN = 26;
static constexpr int MODEM_DTR_PIN = 32;
static constexpr int MODEM_RI_PIN = 33;

static constexpr int I2C_SDA_PIN = 21;
static constexpr int I2C_SCL_PIN = 22;
static constexpr int LED_PIN = 13;

static constexpr int IP5306_ADDR = 0x75;
static constexpr int IP5306_REG_SYS_CTL0 = 0x00;

static constexpr int MPU_ADDR = 0x68;
static constexpr int MPU_INT_PIN = 34;  // Wired by user

static constexpr int IGNITION_PIN = 35; // Wired by user

// Replace with your owner/receiver number.
static constexpr const char* ALERT_PHONE = "+450000000";

static constexpr uint32_t MODEM_BAUD = 115200;
static constexpr uint32_t NETWORK_WAIT_MS = 30000;

static constexpr uint32_t ARMING_GRACE_SECONDS = 31;
static constexpr uint32_t ALERT_COOLDOWN_SECONDS = 15;
static constexpr uint32_t PARK_TIMER_WAKE_SECONDS = 30;

// MUTE N uses minutes. This prevents unbounded mute values by mistake.
static constexpr uint16_t MAX_MUTE_MINUTES = 24 * 60;
