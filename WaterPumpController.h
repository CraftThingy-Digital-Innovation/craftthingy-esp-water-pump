#pragma once

#include <Arduino.h>
#include <CraftThingyCore.h>
#include "PZEM004Tv30.h"

// Hardware Wiring Schemas
enum HardwareWiringSchema {
    SCHEMA_UNKNOWN,
    SCHEMA_LEGACY_G3,      // Standard User: Relay GPIO 0, Float Switch on GPIO 3 (RX), No PZEM
    SCHEMA_PZEM_1UF,       // PZEM User with ~1µF Boot Circuit on GPIO 2, PZEM on UART (TX=1, RX=3)
    SCHEMA_PZEM_10UF,      // PZEM User with ~10µF Boot Circuit on GPIO 2, PZEM on UART (TX=1, RX=3)
    SCHEMA_ESP32_STANDARD  // ESP32 Standard: Relay GPIO 23, Float Switch GPIO 22
};

class WaterPumpController : public IDeviceController {
public:
    WaterPumpController(uint8_t relayPin = 0, uint8_t levelSensorPin = 2, bool activeLowRelay = true, bool switchClosedWhenEmpty = true)
        : _relayPin(relayPin), _levelSensorPin(levelSensorPin), _activeLow(activeLowRelay),
          _switchClosedWhenEmpty(switchClosedWhenEmpty), _pumpRunning(false), _autoMode(true),
          _interrupted(false), _dryRunTripped(false), _pumpStartMs(0), _maxRunTimeMs(900000), // 15 min max run safety
          _lastRawState(HIGH), _debouncedState(HIGH), _lastStateChangeMs(0), _debounceDelayMs(2000), // 2s debounce for water ripples
          _schema(SCHEMA_UNKNOWN), _riseTimeUs(0), _hasPzem(false), _pzemOnline(false),
          _acPowerAlert(false), _lastPzemPollMs(0), _pzemPollIntervalMs(1000), _pzemConsecutiveFailures(0) {
        memset(&_pzemData, 0, sizeof(_pzemData));
    }

    void setup() override {
        // -------------------------------------------------------------
        // 1. Dynamic Hardware Fingerprint & Capacitive Pin Self-Probe
        // -------------------------------------------------------------
        const HardwareFingerprint& fp = ModuleDetector::getFingerprint();

        if (fp.chipType == CHIP_ESP8266_ESP01 || fp.chipType == CHIP_ESP8266_ESP01S || fp.chipType == CHIP_ESP8266_GENERIC) {
            probeEsp01HardwareSchema();
        } else {
            // ESP32 defaults
            _schema = SCHEMA_ESP32_STANDARD;
            _relayPin = 23;
            _levelSensorPin = 22;
            _hasPzem = false;
        }

        // -------------------------------------------------------------
        // 2. Register Roles with PinManager and Configure GPIOs
        // -------------------------------------------------------------
        PinManager::registerRole("relay", "Water Pump Relay", _relayPin, PIN_CAP_OUTPUT);
        PinManager::registerRole("sensor", "Water Level Float Sensor", _levelSensorPin, PIN_CAP_INPUT);

        // Allow EEPROM custom override if explicitly configured
        int8_t configuredRelay = PinManager::getPin("relay");
        int8_t configuredSensor = PinManager::getPin("sensor");
        if (configuredRelay >= 0) _relayPin = (uint8_t)configuredRelay;
        if (configuredSensor >= 0) _levelSensorPin = (uint8_t)configuredSensor;

        pinMode(_relayPin, OUTPUT);
        setRelay(false);

        // Float switch input with pullup enabled
        pinMode(_levelSensorPin, INPUT_PULLUP);
        _debouncedState = digitalRead(_levelSensorPin);
        _lastRawState = _debouncedState;

        // -------------------------------------------------------------
        // 3. PZEM-004T Serial Setup (If PZEM Hardware Detected)
        // -------------------------------------------------------------
        if (_hasPzem) {
#if defined(ESP8266)
            Serial.flush();
            Serial.begin(9600); // Reconfigure hardware UART to 9600 baud for PZEM Modbus
            _pzem.begin(&Serial, 0x01);
#elif defined(ESP32)
            Serial2.begin(9600, SERIAL_8N1, 16, 17);
            _pzem.begin(&Serial2, 0x01);
#endif
            // Initial probe to verify if live AC power is present
            delay(100);
            if (_pzem.read(_pzemData, 300)) {
                _pzemOnline = true;
                _acPowerAlert = false;
                _pzemConsecutiveFailures = 0;
            } else {
                _pzemOnline = false;
                _acPowerAlert = true; // PZEM low-voltage circuit detected, but AC mains not responding!
            }
        }
    }

    void loop() override {
        // -------------------------------------------------------------
        // 1. Water Ripple Debouncing Filter
        // -------------------------------------------------------------
        int rawReading = digitalRead(_levelSensorPin);
        if (rawReading != _lastRawState) {
            _lastRawState = rawReading;
            _lastStateChangeMs = millis();
        }

        if ((millis() - _lastStateChangeMs) >= _debounceDelayMs) {
            _debouncedState = rawReading;
        }

        // Low water triggers contact (e.g. switch closed to GND = LOW)
        bool tankDemandsFill = _switchClosedWhenEmpty ? (_debouncedState == LOW) : (_debouncedState == HIGH);

        // -------------------------------------------------------------
        // 2. Periodic PZEM-004T Power Polling (Non-blocking)
        // -------------------------------------------------------------
        if (_hasPzem && (millis() - _lastPzemPollMs >= _pzemPollIntervalMs)) {
            _lastPzemPollMs = millis();

            if (_pzem.read(_pzemData, 200)) {
                _pzemOnline = true;
                _acPowerAlert = false;
                _pzemConsecutiveFailures = 0;
            } else {
                _pzemConsecutiveFailures++;
                if (_pzemConsecutiveFailures >= 3) {
                    _pzemOnline = false;
                    _acPowerAlert = true; // AC line lost, breaker tripped, or wire loose
                }
            }
        }

        // -------------------------------------------------------------
        // 3. Dry-Run Safety Timer Protection
        // -------------------------------------------------------------
        if (_pumpRunning) {
            if (millis() - _pumpStartMs >= _maxRunTimeMs) {
                setRelay(false);
                _dryRunTripped = true;
            }
        }

        // -------------------------------------------------------------
        // 4. Automatic Water Filling Logic with Mechanical Hysteresis
        // -------------------------------------------------------------
        if (_autoMode && !_dryRunTripped && !_interrupted) {
            if (tankDemandsFill && !_pumpRunning) {
                setRelay(true);
            } else if (!tankDemandsFill && _pumpRunning) {
                setRelay(false);
            }
        }
    }

    const char* getRoleName() const override {
        return "water_pump";
    }

    const char* getAppVersion() const override {
        return "1.1.0";
    }

    const char* getSchemaName() const {
        switch (_schema) {
            case SCHEMA_PZEM_1UF:      return "pzem_1uf";
            case SCHEMA_PZEM_10UF:     return "pzem_10uf";
            case SCHEMA_LEGACY_G3:     return "legacy_g3";
            case SCHEMA_ESP32_STANDARD:return "esp32_standard";
            default:                   return "unknown";
        }
    }

    bool handleCommand(const String& cmd, const String& payload, String& response) override {
        if (cmd == "stop" || cmd == "interrupt" || cmd == "off") {
            _interrupted = true;
            _autoMode = false;
            setRelay(false);
            response = "Operation INTERRUPTED: Pump immediately stopped";
            return true;
        } else if (cmd == "start" || cmd == "on") {
            _interrupted = false;
            _autoMode = false;
            _dryRunTripped = false;
            setRelay(true);
            response = "Pump turned ON manually";
            return true;
        } else if (cmd == "auto" || cmd == "resume") {
            _interrupted = false;
            _autoMode = true;
            _dryRunTripped = false;
            response = "Operation RESUMED: Set to AUTO float-switch mode";
            return true;
        } else if (cmd == "reset_safety") {
            _dryRunTripped = false;
            response = "Safety dry-run alarm reset";
            return true;
        } else if (cmd == "reprobe") {
            setup();
            response = "Hardware re-probed. Detected: " + String(getSchemaName()) + " (Rise time: " + String(_riseTimeUs) + " us)";
            return true;
        }
        return false;
    }

    String getStatusJson() override {
        String json = "{";
        json += "\"role\":\"water_pump\",";
        json += "\"relayPin\":" + String(_relayPin) + ",";
        json += "\"sensorPin\":" + String(_levelSensorPin) + ",";
        json += "\"hardwareSchema\":\"" + String(getSchemaName()) + "\",";
        json += "\"riseTimeUs\":" + String(_riseTimeUs) + ",";
        json += "\"pumpRunning\":" + String(_pumpRunning ? "true" : "false") + ",";
        json += "\"interrupted\":" + String(_interrupted ? "true" : "false") + ",";
        json += "\"autoMode\":" + String(_autoMode ? "true" : "false") + ",";
        json += "\"tankDemandsFill\":" + String((_switchClosedWhenEmpty ? (_debouncedState == LOW) : (_debouncedState == HIGH)) ? "true" : "false") + ",";
        json += "\"dryRunAlarm\":" + String(_dryRunTripped ? "true" : "false") + ",";
        json += "\"runDurationSec\":" + String(_pumpRunning ? (millis() - _pumpStartMs) / 1000 : 0) + ",";
        json += "\"maxRunTimeSec\":" + String(_maxRunTimeMs / 1000) + ",";

        // Telemetry & Diagnostic Status
        json += "\"hasPzem\":" + String(_hasPzem ? "true" : "false") + ",";
        json += "\"pzemOnline\":" + String(_pzemOnline ? "true" : "false") + ",";
        json += "\"acPowerAlert\":" + String(_acPowerAlert ? "true" : "false") + ",";

        if (_hasPzem && _pzemOnline && _pzemData.valid) {
            json += "\"voltage\":" + String(_pzemData.voltage, 1) + ",";
            json += "\"current\":" + String(_pzemData.current, 3) + ",";
            json += "\"power\":" + String(_pzemData.power, 1) + ",";
            json += "\"energy\":" + String(_pzemData.energy, 3) + ",";
            json += "\"frequency\":" + String(_pzemData.frequency, 1) + ",";
            json += "\"powerFactor\":" + String(_pzemData.powerFactor, 2);
        } else {
            json += "\"voltage\":0.0,";
            json += "\"current\":0.0,";
            json += "\"power\":0.0,";
            json += "\"energy\":0.0,";
            json += "\"frequency\":0.0,";
            json += "\"powerFactor\":0.0";
        }

        json += "}";
        return json;
    }

    bool isSafeToUpdate() const override {
        return !_pumpRunning;
    }

    void prepareForUpdate() override {
        setRelay(false);
    }

private:
    uint8_t _relayPin;
    uint8_t _levelSensorPin;
    bool _activeLow;
    bool _switchClosedWhenEmpty;
    bool _pumpRunning;
    bool _autoMode;
    bool _interrupted;
    bool _dryRunTripped;
    uint32_t _pumpStartMs;
    uint32_t _maxRunTimeMs;
    int _lastRawState;
    int _debouncedState;
    uint32_t _lastStateChangeMs;
    uint32_t _debounceDelayMs;

    // Hardware Auto-Detection & PZEM-004T state
    HardwareWiringSchema _schema;
    uint32_t _riseTimeUs;
    bool _hasPzem;
    bool _pzemOnline;
    bool _acPowerAlert;
    uint32_t _lastPzemPollMs;
    uint32_t _pzemPollIntervalMs;
    uint8_t _pzemConsecutiveFailures;
    PZEM004Tv30 _pzem;
    PZEMData _pzemData;

    void setRelay(bool on) {
        _pumpRunning = on;
        if (on) _pumpStartMs = millis();
        digitalWrite(_relayPin, _activeLow ? (!on ? HIGH : LOW) : (on ? HIGH : LOW));
    }

    /**
     * @brief Measures RC rise-time on GPIO 2 to detect bare pin vs 1uF vs 10uF boot circuit.
     */
    void probeEsp01HardwareSchema() {
#if defined(ESP8266)
        // Step 1: Fully drain any residual charge on GPIO 2 to GND
        pinMode(2, OUTPUT);
        digitalWrite(2, LOW);
        delay(10); // 10ms is sufficient to drain 1uF and 10uF

        // Step 2: Switch to INPUT_PULLUP and time the rise to logic HIGH
        pinMode(2, INPUT_PULLUP);
        uint32_t startUs = micros();
        while (digitalRead(2) == LOW && (micros() - startUs) < 35000) {
            // Wait for voltage to cross logic HIGH threshold (or 35ms timeout)
        }
        _riseTimeUs = micros() - startUs;

        // Step 3: Classify based on measured electrical rise-time:
        // - Bare pin (Standard Legacy): ~0 to 30 µs (due to stray ~5pF board capacitance)
        // - 1µF Boot Circuit: ~150 to 7,000 µs
        // - 10µF Boot Circuit: >= 7,000 µs (typically 12,000 - 35,000 µs)
        if (_riseTimeUs < 100) {
            _schema = SCHEMA_LEGACY_G3;
            _relayPin = 0;
            _levelSensorPin = 3; // Float switch on GPIO 3 (RX)
            _hasPzem = false;

            // Close Serial so GPIO 3 is dedicated to float switch
            Serial.flush();
            Serial.end();
            pinMode(3, INPUT_PULLUP);
        } else if (_riseTimeUs < 7000) {
            _schema = SCHEMA_PZEM_1UF;
            _relayPin = 0;
            _levelSensorPin = 2; // Float switch on GPIO 2
            _hasPzem = true;
        } else {
            _schema = SCHEMA_PZEM_10UF;
            _relayPin = 0;
            _levelSensorPin = 2; // Float switch on GPIO 2
            _hasPzem = true;
        }
#endif
    }
};
