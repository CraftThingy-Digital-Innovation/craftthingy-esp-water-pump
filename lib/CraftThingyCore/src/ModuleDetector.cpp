#include "ModuleDetector.h"
#include <Wire.h>

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
    #include <esp_chip_info.h>
#endif

HardwareFingerprint ModuleDetector::_fingerprint;

void ModuleDetector::init() {
    identifyChipAndFlash();
    probePeripherals();
}

void ModuleDetector::identifyChipAndFlash() {
    memset(&_fingerprint, 0, sizeof(HardwareFingerprint));

#if defined(ESP8266)
    _fingerprint.flashSizeBytes = ESP.getFlashChipRealSize();
    _fingerprint.flashChipId = ESP.getFlashChipId();
    _fingerprint.freeHeapBytes = ESP.getFreeHeap();
    _fingerprint.macAddress = WiFi.macAddress();

    // Distinguish ESP-01 from ESP-01S based on flash size and chip properties
    if (_fingerprint.flashSizeBytes == 1048576) { // Exactly 1MB Flash
        // Modern ESP-01S typically carries 1MB flash
        _fingerprint.chipType = CHIP_ESP8266_ESP01S;
        _fingerprint.chipName = "ESP8266 (ESP-01S 1MB)";
        _fingerprint.archTag = "esp01s";
    } else if (_fingerprint.flashSizeBytes <= 524288) { // 512KB Flash
        _fingerprint.chipType = CHIP_ESP8266_ESP01;
        _fingerprint.chipName = "ESP8266 (ESP-01 512KB)";
        _fingerprint.archTag = "esp01";
    } else { // 4MB or larger (NodeMCU, D1 Mini, ESP-12F)
        _fingerprint.chipType = CHIP_ESP8266_GENERIC;
        _fingerprint.chipName = "ESP8266 Generic (ESP-12/NodeMCU)";
        _fingerprint.archTag = "esp8266";
    }

#elif defined(ESP32)
    _fingerprint.flashSizeBytes = ESP.getFlashChipSize();
    _fingerprint.freeHeapBytes = ESP.getFreeHeap();
    _fingerprint.macAddress = WiFi.macAddress();

    #if defined(CONFIG_IDF_TARGET_ESP32S3)
        _fingerprint.chipType = CHIP_ESP32_S3;
        _fingerprint.chipName = "ESP32-S3";
        _fingerprint.archTag = "esp32s3";
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        _fingerprint.chipType = CHIP_ESP32_C3;
        _fingerprint.chipName = "ESP32-C3";
        _fingerprint.archTag = "esp32c3";
    #elif defined(CONFIG_IDF_TARGET_ESP32C6)
        _fingerprint.chipType = CHIP_ESP32_C6;
        _fingerprint.chipName = "ESP32-C6";
        _fingerprint.archTag = "esp32c6";
    #elif defined(CONFIG_IDF_TARGET_ESP32S2)
        _fingerprint.chipType = CHIP_ESP32_S2;
        _fingerprint.chipName = "ESP32-S2";
        _fingerprint.archTag = "esp32s2";
    #else
        _fingerprint.chipType = CHIP_ESP32_ORIGINAL;
        _fingerprint.chipName = "ESP32 Standard (Xtensa LX6)";
        _fingerprint.archTag = "esp32";
    #endif

#endif

    _fingerprint.detectedRole = ROLE_UNASSIGNED;
    _fingerprint.roleString = "unassigned";
}

String ModuleDetector::scanI2cPeripherals(uint8_t sdaPin, uint8_t sclPin) {
    Wire.begin(sdaPin, sclPin);
    String detected = "";
    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (count > 0) detected += ",";
            char hexBuf[5];
            snprintf(hexBuf, sizeof(hexBuf), "0x%02X", addr);
            detected += String(hexBuf);
            count++;
        }
        delay(2);
    }
    return detected;
}

void ModuleDetector::probePeripherals() {
#if defined(ESP8266)
    // On ESP-01 / ESP-01S, GPIO0 (SDA) and GPIO2 (SCL) are often used for I2C if connected
    String i2cDevices = scanI2cPeripherals(0, 2);
    _fingerprint.detectedPeripheral = i2cDevices.length() > 0 ? ("I2C:" + i2cDevices) : "None";

    // Heuristics: if ADS1115 (0x48) or PCF8574 (0x20/0x27/0x3F) relay found
    if (i2cDevices.indexOf("0x48") >= 0 || i2cDevices.indexOf("0x27") >= 0 || i2cDevices.indexOf("0x3F") >= 0) {
        _fingerprint.detectedRole = ROLE_WATER_PUMP;
        _fingerprint.roleString = "water_pump";
    } else {
        _fingerprint.detectedRole = ROLE_UNASSIGNED;
        _fingerprint.roleString = "generic";
    }

#elif defined(ESP32)
    #if defined(CONFIG_IDF_TARGET_ESP32C3)
        String i2cDevices = scanI2cPeripherals(4, 5);
    #elif defined(CONFIG_IDF_TARGET_ESP32S3)
        String i2cDevices = scanI2cPeripherals(8, 9);
    #else
        String i2cDevices = scanI2cPeripherals(21, 22);
    #endif

    _fingerprint.detectedPeripheral = i2cDevices.length() > 0 ? ("I2C:" + i2cDevices) : "None";

    if (i2cDevices.indexOf("0x48") >= 0) {
        _fingerprint.detectedRole = ROLE_WATER_PUMP;
        _fingerprint.roleString = "water_pump";
    } else if (i2cDevices.indexOf("0x68") >= 0 || i2cDevices.indexOf("0x76") >= 0) {
        _fingerprint.detectedRole = ROLE_WASHING_MACHINE;
        _fingerprint.roleString = "washing_machine";
    } else {
        _fingerprint.detectedRole = ROLE_UNASSIGNED;
        _fingerprint.roleString = "generic";
    }
#endif
}

const HardwareFingerprint& ModuleDetector::getFingerprint() {
    return _fingerprint;
}

String ModuleDetector::resolveAssetFilename(const String& roleName) {
    // Standardized asset filename:
    // e.g. "firmware-water_pump-esp01s.bin", "firmware-washing_machine-esp32.bin"
    // Also matches generic pattern "firmware-esp01s.bin" or "firmware-esp32.bin"
    String role = roleName.length() > 0 ? roleName : String(_fingerprint.roleString);
    role.toLowerCase();
    role.replace(" ", "_");

    return "firmware-" + role + "-" + String(_fingerprint.archTag) + ".bin";
}

String ModuleDetector::resolveRepositoryName(const String& roleName) {
    String role = roleName.length() > 0 ? roleName : String(_fingerprint.roleString);
    role.toLowerCase();

    if (role == "water_pump" || role == "pump") {
        return "water-pump-controller";
    } else if (role == "washing_machine" || role == "washer") {
        return "washing-machine-controller";
    } else if (role == "core" || role == "updater") {
        return "craftthingy-esp-core-ota";
    }

    return role + "-controller";
}

String ModuleDetector::getSummaryJson() {
    String json = "{";
    json += "\"chip\":\"" + String(_fingerprint.chipName) + "\",";
    json += "\"archTag\":\"" + String(_fingerprint.archTag) + "\",";
    json += "\"flashBytes\":" + String(_fingerprint.flashSizeBytes) + ",";
    json += "\"freeHeap\":" + String(_fingerprint.freeHeapBytes) + ",";
    json += "\"mac\":\"" + _fingerprint.macAddress + "\",";
    json += "\"peripherals\":\"" + _fingerprint.detectedPeripheral + "\",";
    json += "\"detectedRole\":\"" + String(_fingerprint.roleString) + "\"";
    json += "}";
    return json;
}
