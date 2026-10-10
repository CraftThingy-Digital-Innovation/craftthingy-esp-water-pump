#pragma once

#include <Arduino.h>
#include "../DeviceController.h"

class WaterPumpController : public IDeviceController {
public:
    WaterPumpController(uint8_t relayPin = 0, uint8_t levelSensorPin = 2, bool activeLow = true)
        : _relayPin(relayPin), _levelSensorPin(levelSensorPin), _activeLow(activeLow),
          _pumpRunning(false), _autoMode(true), _dryRunTripped(false),
          _pumpStartMs(0), _maxRunTimeMs(900000) {} // 15 min max run safety

    void setup() override {
        pinMode(_relayPin, OUTPUT);
        setRelay(false);

#if defined(ESP8266)
        // GPIO2 has internal pullup on ESP-01S
        pinMode(_levelSensorPin, INPUT_PULLUP);
#elif defined(ESP32)
        pinMode(_levelSensorPin, INPUT_PULLUP);
#endif
    }

    void loop() override {
        // Read water level (LOW = water full/float up, HIGH = tank empty)
        bool tankEmpty = (digitalRead(_levelSensorPin) == HIGH);

        // Dry-run safety timer protection
        if (_pumpRunning) {
            if (millis() - _pumpStartMs >= _maxRunTimeMs) {
                setRelay(false);
                _dryRunTripped = true;
            }
        }

        // Automatic water filling logic
        if (_autoMode && !_dryRunTripped) {
            if (tankEmpty && !_pumpRunning) {
                setRelay(true);
            } else if (!tankEmpty && _pumpRunning) {
                setRelay(false);
            }
        }
    }

    const char* getRoleName() const override {
        return "water_pump";
    }

    const char* getAppVersion() const override {
        return "1.0.0";
    }

    bool handleCommand(const String& cmd, const String& payload, String& response) override {
        if (cmd == "on") {
            _autoMode = false;
            _dryRunTripped = false;
            setRelay(true);
            response = "Pump turned ON manually";
            return true;
        } else if (cmd == "off") {
            _autoMode = false;
            setRelay(false);
            response = "Pump turned OFF manually";
            return true;
        } else if (cmd == "auto") {
            _autoMode = true;
            _dryRunTripped = false;
            response = "Pump set to AUTO mode";
            return true;
        } else if (cmd == "reset_safety") {
            _dryRunTripped = false;
            response = "Safety dry-run alarm reset";
            return true;
        }
        return false;
    }

    String getStatusJson() override {
        String json = "{";
        json += "\"pumpState\":\"" + String(_pumpRunning ? "running" : "stopped") + "\",";
        json += "\"autoMode\":" + String(_autoMode ? "true" : "false") + ",";
        json += "\"dryRunAlarm\":" + String(_dryRunTripped ? "true" : "false") + ",";
        json += "\"tankEmpty\":" + String(digitalRead(_levelSensorPin) == HIGH ? "true" : "false") + ",";
        json += "\"runDurationSec\":" + String(_pumpRunning ? (millis() - _pumpStartMs) / 1000 : 0);
        json += "}";
        return json;
    }

private:
    uint8_t _relayPin;
    uint8_t _levelSensorPin;
    bool _activeLow;
    bool _pumpRunning;
    bool _autoMode;
    bool _dryRunTripped;
    uint32_t _pumpStartMs;
    uint32_t _maxRunTimeMs;

    void setRelay(bool on) {
        _pumpRunning = on;
        if (on) _pumpStartMs = millis();
        digitalWrite(_relayPin, _activeLow ? (!on ? HIGH : LOW) : (on ? HIGH : LOW));
    }
};
