#pragma once

#include <Arduino.h>
#include "CraftThingyConfig.h"

// Detected Hardware Architecture & Variant
enum HardwareChipType {
    CHIP_UNKNOWN,
    CHIP_ESP8266_ESP01,     // ESP-01 (older 512KB/1MB flash)
    CHIP_ESP8266_ESP01S,    // ESP-01S (1MB flash PUYA/Winbond, modern layout)
    CHIP_ESP8266_GENERIC,   // ESP-12E/F, NodeMCU, D1 Mini
    CHIP_ESP32_ORIGINAL,    // Standard ESP32 (Xtensa Dual-Core)
    CHIP_ESP32_S2,          // ESP32-S2 (Xtensa Single-Core)
    CHIP_ESP32_S3,          // ESP32-S3 (Xtensa Dual-Core + USB/AI)
    CHIP_ESP32_C3,          // ESP32-C3 (RISC-V)
    CHIP_ESP32_C6           // ESP32-C6 (RISC-V WiFi 6 + Zigbee/Thread)
};

// Auto-detected or Assigned Functional Module Role
enum ModuleRoleType {
    ROLE_UNASSIGNED,
    ROLE_WATER_PUMP,        // Water pump / tank controller
    ROLE_WASHING_MACHINE,   // Washing machine controller
    ROLE_RELAY_CONTROLLER,  // Smart switch / multi-channel relay
    ROLE_SENSOR_NODE,       // Environmental / water level sensor
    ROLE_CUSTOM             // Custom user-defined module
};

struct HardwareFingerprint {
    HardwareChipType chipType;
    const char* chipName;
    const char* archTag;        // e.g. "esp01s", "esp01", "esp32", "esp32s3", "esp32c3"
    uint32_t flashSizeBytes;
    uint32_t flashChipId;
    uint32_t freeHeapBytes;
    String macAddress;
    String detectedPeripheral;  // e.g. "I2C:0x38,0x68" or "ADC_STRAP:PumpCarrier"
    ModuleRoleType detectedRole;
    const char* roleString;
};

class ModuleDetector {
public:
    static void init();

    // Run hardware & peripheral detection
    static HardwareFingerprint detect();

    // Get cached fingerprint
    static const HardwareFingerprint& getFingerprint();

    // Dynamic asset filename resolution: e.g. "waterpump-esp01s-1mb.bin" or "craftthingy-esp32.bin"
    static String resolveAssetFilename(const String& roleName);

    // Dynamic repository name resolution: e.g. "CraftThingy-WaterPump" or custom
    static String resolveRepositoryName(const String& roleName);

    // Scan I2C bus for hardware module identifiers
    static String scanI2cPeripherals(uint8_t sdaPin, uint8_t sclPin);

    // Human-readable summary
    static String getSummaryJson();

private:
    static HardwareFingerprint _fingerprint;
    static void identifyChipAndFlash();
    static void probePeripherals();
};
