# BikeGuard

BikeGuard is a motorcycle/car anti-theft project with two firmware tracks:

- A9G track (most complete right now)
- ESP32 + SIM800L track (still under development)

This README reflects what currently exists in this repository.

## Repository Layout

- `src/a9g/src/bikeguard.c`: A9G implementation (single integrated source file)
- `src/a9g/Makefile`: A9G demo build file (for Ai-Thinker GPRS C SDK workflow)
- `src/esp/`: ESP modules (`config`, `ignition`, `motion`, `sms`, `sleep`)
- `platformio.ini`: ESP PlatformIO environments

## Hardware Section: A9G (Current Main Logic)

Status: active and feature-rich prototype.

### What it does now

From the current logic in `src/a9g/src/bikeguard.c`:

1. Boots and waits for system + network ready events.
2. Initializes SMS stack when the network is registered.
3. Reads MPU6050 accelerometer over I2C2.
4. Reads ignition input on GPIO25.
5. Uses arming grace (20s) after ignition goes OFF.
6. Learns motion baseline, then checks delta threshold.
7. Requires consecutive motion hits before alarm.
8. Sends SMS alert with motion values.
9. Opens GPS on alarm, waits for fix (up to 30s), appends Google Maps link if available.
10. Applies SMS cooldown (15s) to reduce spam.
11. Supports remote mute command from owner number:
   - `MUTE <minutes>`
   - `MUTE 0` to clear mute

### A9G Behavior Summary

- Guard OFF when ignition is ON.
- Guard arms after ignition OFF + grace period.
- Motion while armed triggers SMS (unless muted/cooldown).
- GPS is opened only during alert flow, then closed for better power behavior.

### A9G Wiring Logic

This reflects the current A9G logic in `src/a9g/src/bikeguard.c`:

- MPU6050 is read on I2C2 at address `0x68`
- Ignition sense input is on `GPIO25`

Recommended wiring logic:

```text
Bike 12V Battery (+) --------------------> Buck Converter IN+
Bike GND --------------------------------> Buck Converter IN-

Buck 5V OUT+ ----------------------------> A9G VUSB (or board 5V input)
Buck GND --------------------------------> A9G GND

Bike switched 12V (ignition line)
    |
   47k
    |
    +-------------------------------------> A9G GPIO25 (ignition sense)
    |
   10k
    |
   GND

MPU6050 VCC -----------------------------> A9G 3V3 (or module-safe VCC)
MPU6050 GND -----------------------------> A9G GND
MPU6050 SDA -----------------------------> A9G I2C2_SDA pin
MPU6050 SCL -----------------------------> A9G I2C2_SCL pin
MPU6050 AD0 -----------------------------> GND (addr 0x68) or 3V3 (addr 0x69)
```

Notes:

- Keep all grounds common (bike power, A9G, MPU6050).
- The ignition line must go through a divider before entering GPIO.
- Use a stable supply path; GSM burst current can be high.

### A9G Programming App and SDK

For A9G, PlatformIO is not the flashing toolchain. Use:

1. Editor: VS Code (or any editor) for code changes.
2. Compiler/Build system: Ai-Thinker GPRS C SDK.
3. Flash/Download tool: CoolWatcher/serial download flow used by the SDK ecosystem.

Official SDK links:

- SDK repo: https://github.com/Ai-Thinker-Open/GPRS_C_SDK
- SDK releases: https://github.com/Ai-Thinker-Open/GPRS_C_SDK/releases
- SDK docs: https://ai-thinker-open.github.io/GPRS_C_SDK_DOC/zh/

Typical Windows compile flow:

```bash
cd GPRS_C_SDK
build.bat demo motion_test
```

That generates firmware artifacts from the demo module (for example `.lod`), then you flash using the A9G download/debug tooling.

## Hardware Section: ESP32 + SIM800L (Under Development)

Status: under development, not final.

Current ESP source exists in `src/esp/` and includes:

- `motion.cpp`: MPU6050 initialization and delta-based movement detection
- `ignition.cpp`: ignition sensing input logic
- `sms.cpp`: SIM800L init/send/sleep/wake abstraction
- `sleep.cpp`: light sleep helper
- `config.h`: pins, phone number, and motion/sleep tuning constants

### Important note

The ESP folder contains building blocks and partial behavior, but it is not marked complete yet. Treat it as work in progress compared to the A9G flow.

## Build and Development

### ESP (PlatformIO)

Use PlatformIO environments from `platformio.ini`:

```bash
pio run
pio run --target upload
pio device monitor
```

### A9G

A9G code here is structured for Ai-Thinker GPRS C SDK style builds (`demo/motion_test` style Makefile). It is not flashed from PlatformIO directly.

## Future Improvements (To Add Later)

### 1) Receiver number update over SMS with PIN code

Add secure owner/receiver update command, for example:

- `SETPHONE <PIN> <NUMBER>`

Expected behavior:

- Validate sender and PIN.
- Update stored receiver number.
- Persist it so it survives reboot.
- Send ACK/NACK SMS with reason.

### 2) Current status command over SMS

Add status request command, for example:

- `STATUS <PIN>`

Status reply should include:

- Location (if GPS fix is available)
- Power source (main power vs battery)
- Battery percentage (if measurable)
- Battery voltage

Suggested unified status format:

- `STATUS: power=BATTERY, battery=62%, vbat=3.92V, gps=55.6761,12.5683`
- If no fix: `gps=NA`

### 3) Low battery warning SMS at thresholds

When running on battery, send warning SMS at:

- 40%
- 30%
- 20%
- 10%

Each warning message should use the same format as the STATUS response so it is consistent and easy to parse/read.

Example:

- `STATUS: power=BATTERY, battery=30%, vbat=3.70V, gps=55.6761,12.5683`

## Project Direction

- Keep A9G as the reference implementation for full alarm behavior.
- Continue ESP implementation until feature parity is reached.
- Add secure SMS command set and battery-aware reporting before production use.
