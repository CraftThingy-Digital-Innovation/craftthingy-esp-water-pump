# CraftThingy - Automated Water Pump Controller

Modular water pump controller firmware with dry-run protection, tank level monitoring, and auto-updating capabilities powered by `craftthingy-esp-core-ota`.

## Hardware Wiring

### ESP8266 (ESP-01 / ESP-01S 1MB Flash)
- **GPIO 0**: Relay Signal (Active LOW for standard ESP-01 relay shield)
- **GPIO 2**: Float Switch / Level Sensor (Internal pull-up enabled, LOW = Full, HIGH = Empty)
- **VCC**: 3.3V
- **GND**: Common Ground

### ESP32
- **GPIO 23**: Relay Signal
- **GPIO 22**: Float Switch / Level Sensor

## Building
```bash
# Build for ESP-01 / ESP-01S
pio run -e esp01_1m

# Build for ESP32
pio run -e esp32dev
```
