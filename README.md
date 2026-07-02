# BikeGuard

Motorcycle theft detector built on the TTGO T-Call V1.3 (ESP32 + SIM800L). Detects movement when the ignition is off and sends an SMS alert.

## Hardware

| Component | Role | Status |
|---|---|---|
| TTGO T-Call V1.3 | ESP32 brain + SIM800L for SMS | Purchased |
| MPU6050 (GY-521) | Motion detection via accelerometer | Purchased |
| MP1584EN buck converter | 12V to 5V power from bike battery | Purchased |
| LiPo 3.7V, 500-1000mAh, JST-PH 2mm | Backup power when bike is disconnected | To buy |
| 47kΩ + 10kΩ resistors | Ignition voltage divider | To buy |
| Dupont jumper wires | Prototype wiring | To buy |
| Lebara DK nano SIM | SMS carrier | To buy |
| Project enclosure | Weather protection | To buy |

## Wiring

| Connection | GPIO |
|---|---|
| MPU6050 SDA | 14 |
| MPU6050 SCL | 15 |
| Ignition sense | 35 |

**Ignition sense circuit** — 12V switched line through a voltage divider before GPIO 35:
```
12V (switched) ── 47kΩ ──┬── GPIO 35
                         │
                        10kΩ
                         │
                        GND
```

That divider gives about 2.1V at GPIO 35 from a 12V line, and about 2.5V when the charging system is closer to 14.4V. Keep the 47kΩ resistor on the 12V side and the 10kΩ resistor on the ground side.

Reserved TTGO T-Call GPIOs: 4, 5, 21, 22, 23, 26, 27.

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
