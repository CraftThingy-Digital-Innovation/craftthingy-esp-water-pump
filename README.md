# CraftThingy ESP Water Pump Controller (`craftthingy-esp-water-pump`)

[![Hardware](https://img.shields.io/badge/Hardware-ESP--01%20%7C%20ESP--01S%20%7C%20ESP32-blue.svg)]()
[![Role](https://img.shields.io/badge/Role-Water%20Pump%20Automation-teal.svg)]()
[![Safety](https://img.shields.io/badge/Safety-Dry--Run%20Protection%20Timer-green.svg)]()
[![OTA](https://img.shields.io/badge/OTA-Dual%20GitHub%20Releases-orange.svg)]()

Automated water pump controller firmware engineered for **ESP-01 / ESP-01S (1MB Flash)** and **ESP32**. Features automatic water tank level filling, float switch sensing, emergency dry-run protection timers, and remote REST API control with automatic updates powered by `craftthingy-esp-core-ota`.

---

## 📑 Table of Contents
1. [Features](#-features)
2. [Hardware Wiring & Pinouts](#-hardware-wiring--pinouts)
3. [Safety & Operation Logic](#-safety--operation-logic)
4. [REST API Control](#-rest-api-control)
5. [Self-Updating Architecture](#-self-updating-architecture)
6. [Arduino IDE Guide](#-arduino-ide-guide)
7. [PlatformIO Guide](#-platformio-guide)
8. [Automated CI/CD Workflow](#-automated-cicd-workflow)

---

## 🌟 Features

- **Automated Level Control**: Activates pump when the tank drops below the threshold and shuts off when full.
- **Dry-Run Safety Protection**: If the pump runs continuously for more than 15 minutes without the water level rising, it shuts down immediately and trips a safety alarm to prevent motor burnout.
- **Failover AP Provisioning**: Automatically broadcasts `CraftThingy-XXXX` if WiFi connection fails, allowing easy configuration.
- **HMAC-SHA256 Rolling Key Security**: Reconfigure WiFi or pump targets securely without static passwords.
- **Selective Binary OTA**: Downloads only its own board binary (`firmware-water_pump-esp01s.bin`), never cluttering flash memory.

---

## 🔌 Hardware Wiring & Pinouts

### 1. ESP8266 (ESP-01 / ESP-01S) — Dynamic Auto-Detecting Schemas

The firmware automatically senses the physical wiring on boot using capacitive pin fingerprinting:

#### A. Standard / Legacy Schema (`legacy_g3`)
*Detected when GPIO 2 is an empty, unpopulated pin.*
* **GPIO 0**: Relay Signal (`IN`)
* **GPIO 3 (RX)**: Float Switch / Level Sensor (Direct to GND)
* **PZEM**: Disabled (Hardware Serial preserved for USB)

#### B. Power Monitoring Schema (`pzem_1uf` or `pzem_10uf`)
*Detected when an RC boot-delay circuit is installed on GPIO 2.*
* **GPIO 0**: Relay Signal (`IN`)
* **GPIO 2**: Float Switch with Boot-Delay Circuit (Supports **1µF** or **10µF** capacitor)
* **GPIO 1 (TX)**: PZEM-004T RX
* **GPIO 3 (RX)**: PZEM-004T TX
* **Telemetry**: Voltage, Current, Power, Energy, Frequency, Power Factor, and AC Power-Loss Alarm.

```text
               +-------------------+
               |    ESP-01 / 01S   |
               |  GND   TX    GPIO2| ----> [Boot Circuit + Float Switch]
               |  GPIO0 EN    RX   | <---> [PZEM-004T (TX/RX)] / Relay IN
               |  VCC              | ----> +3.3V Power
               +-------------------+
```

### 2. ESP32 (DevKit / WROOM / S3 / C3)

| ESP32 Pin | Connected Device | Notes |
|---|---|---|
| **GPIO 23** | Relay Signal (`IN`) | Active LOW / High configurable |
| **GPIO 22** | Float Switch | Internal pull-up enabled |
| **VIN / 5V** | Power (+5V) | Onboard regulator supplies 3.3V |
| **GND** | Ground | Common ground |

---

## 🛡️ Safety & Operation Logic

```text
Tank Empty Detected (GPIO2 = HIGH)
         │
         ▼
Pump Turns ON & Timer Starts
         │
         ├── Tank Full Detected (GPIO2 = LOW) ──► Turn Pump OFF (Normal Auto Fill)
         │
         └── Run Time > 15 Minutes ─────────────► TRIP DRY-RUN SAFETY ALARM!
                                                  Turn Pump OFF immediately.
                                                  Requires manual reset.
```

### Dry-Run Protection
If the water source runs dry or a pipe bursts, a pump running continuously will overheat and burn out. The built-in safety timer trips if runtime exceeds `_maxRunTimeMs` (default: 15 minutes = 900,000 ms). Once tripped, the pump remains locked out until explicitly reset via REST API or reboot.

---

## 🌐 Standardized REST API Control (`v1`, Port 80)

All endpoints require **authentication** (HMAC-SHA256 signature or Bearer token) for physical electrical safety. Responses are wrapped in a standard JSend envelope:

### 1. Check Pump Telemetry (`GET /api/v1/device/status` or `/api/v1/pump/status`)
Returns tank fill demand, debounced float sensor state, active mode, runtime, and safety alarms:
```json
{
  "status": "success",
  "code": 200,
  "uptime": 1240,
  "data": {
    "role": "water_pump",
    "pumpRunning": true,
    "interrupted": false,
    "autoMode": true,
    "tankDemandsFill": true,
    "dryRunAlarm": false,
    "runDurationSec": 42,
    "maxRunTimeSec": 900
  }
}
```

### 2. Send Control Command (`POST /api/v1/device/command` or `/api/v1/pump/command`)
Send authenticated POST command via Python CLI or zero-install browser dashboard [`tools/esp_manager.html`](file:///d:/CraftThingy/esp-stuff/tools/esp_manager.html):

```json
{
  "cmd": "interrupt"
}
```

Supported Commands:
- `interrupt` (or `stop`, `off`): **EMERGENCY STOP**. Instantly disengages the pump relay and locks auto-mode off until explicitly resumed.
- `start` (or `on`): Manually forces the pump relay ON (switches to manual mode).
- `auto` (or `resume`): Resumes autonomous float switch hysteresis filling.
- `reset_safety`: Resets the 15-minute dry-run safety lockout alarm.

---

## 🔄 Self-Updating Architecture

The device automatically checks two repositories in organization `CraftThingy-Digital-Innovation`:
1. `craftthingy-esp-core-ota`: For framework and WiFi security updates.
2. `craftthingy-esp-water-pump`: For pump logic and timing algorithm updates.

When querying GitHub Releases, an ESP-01S matches only `firmware-water_pump-esp01s.bin`, ignoring all other assets.

---

## 🔨 Arduino IDE Guide

1. Open Arduino IDE.
2. Open [`craftthingy-esp-water-pump.ino`](file:///d:/CraftThingy/esp-stuff/craftthingy-esp-water-pump/craftthingy-esp-water-pump.ino).
3. Board Configuration:
   - Board: `Generic ESP8266 Module`
   - Flash Size: `1MB (FS:64KB OTA:~470KB)`
4. Connect your programmer (USB to TTL / FTDI) and click **Upload**.

---

## 💻 PlatformIO Guide

```bash
# Build for ESP-01 / ESP-01S (1MB)
pio run -e esp01_1m

# Build for ESP32
pio run -e esp32dev

# Upload firmware
pio run -e esp01_1m -t upload
```

---

## 🤖 Automated CI/CD Workflow

Pushing a Git tag triggers [`.github/workflows/release.yml`](file:///d:/CraftThingy/esp-stuff/craftthingy-esp-water-pump/.github/workflows/release.yml):
```bash
git tag v1.0.0
git push origin v1.0.0
```
GitHub Actions automatically compiles `firmware-water_pump-esp01s.bin` and `firmware-water_pump-esp32.bin`, calculates SHA-256 checksums, and attaches them to the GitHub Release.
