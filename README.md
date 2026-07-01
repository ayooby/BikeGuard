# BikeGuard

Motorcycle theft detector built on the TTGO T-Call V1.3 (ESP32 + SIM800L). Detects movement when the ignition is off and sends an SMS alert.

## Hardware

| Component | Role |
|---|---|
| TTGO T-Call V1.3 | ESP32 brain + SIM800L for SMS |
| MPU6050 (GY-521) | Motion detection via accelerometer |
| MP1584EN buck converter | 12V → 5V power from bike battery |
| LiPo 3.7V (JST-PH 2mm) | Backup power when bike is disconnected |

## Wiring

| Connection | GPIO |
|---|---|
| MPU6050 SDA | 14 |
| MPU6050 SCL | 15 |
| Ignition sense | 35 |

**Ignition sense circuit** — 12V switched line through a voltage divider before GPIO 35:
```
12V (switched) ── 10kΩ ──┬── GPIO 35
                         │
                        47kΩ
                         │
                        GND
```

Power the board from the always-on 12V (before the key switch) through the buck converter. The key-switched line is only used as a signal.

## How it works

```
IDLE (ignition on)
  └─ ignition off → ARMED
        └─ movement detected → TIMING (10s timeout)
              └─ still moving → ALERT → SMS sent
              └─ movement stops → ARMED
        └─ ignition on → IDLE
```

The timeout avoids false alerts when putting the bike on the sidestand or loading it.

## Config

Edit `src/config.h` before flashing:

```c
#define ALERT_PHONE  "+45xxxxxxxx"   // your number
```

All thresholds and pin assignments are in the same file.

## Development

**Requirements:** VS Code + PlatformIO extension

```bash
# Build
pio run

# Flash
pio run --target upload

# Serial monitor (115200 baud)
pio device monitor
```

State transitions are printed to serial so you can verify behaviour before mounting on the bike.

## Roadmap

- [ ] GPS location in SMS alert
- [ ] Backup LiPo auto-arm when main power is cut
