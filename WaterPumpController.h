#pragma once

#include <Arduino.h>
#include <CraftThingyCore.h>

class WaterPumpController : public IDeviceController {
public:
    WaterPumpController(uint8_t relayPin = 0, uint8_t levelSensorPin = 2, bool activeLowRelay = true, bool switchClosedWhenEmpty = true)
        : _relayPin(relayPin), _levelSensorPin(levelSensorPin), _activeLow(activeLowRelay),
          _switchClosedWhenEmpty(switchClosedWhenEmpty), _pumpRunning(false), _autoMode(true),
          _interrupted(false), _dryRunTripped(false), _pumpStartMs(0), _maxRunTimeMs(900000), // 15 min max run safety
          _lastRawState(HIGH), _debouncedState(HIGH), _lastStateChangeMs(0), _debounceDelayMs(2000) {} // 2s debounce for water ripples

    void setup() override {
        pinMode(_relayPin, OUTPUT);
        setRelay(false);

        // Internal pull-up: Switch connects between this pin and GND
        pinMode(_levelSensorPin, INPUT_PULLUP);
        _debouncedState = digitalRead(_levelSensorPin);
        _lastRawState = _debouncedState;
    }

    void loop() override {
        // 1. Ripple debouncing filter (averages water waves / mechanical contact bounce)
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

        // 2. Dry-run safety timer protection
        if (_pumpRunning) {
            if (millis() - _pumpStartMs >= _maxRunTimeMs) {
                setRelay(false);
                _dryRunTripped = true;
            }
        }

        // 3. Automatic water filling logic with mechanical float hysteresis
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
        return "1.0.0";
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
        }
        return false;
    }

    String getStatusJson() override {
        String json = "{";
        json += "\"role\":\"water_pump\",";
        json += "\"pumpRunning\":" + String(_pumpRunning ? "true" : "false") + ",";
        json += "\"interrupted\":" + String(_interrupted ? "true" : "false") + ",";
        json += "\"autoMode\":" + String(_autoMode ? "true" : "false") + ",";
        json += "\"tankDemandsFill\":" + String((_switchClosedWhenEmpty ? (_debouncedState == LOW) : (_debouncedState == HIGH)) ? "true" : "false") + ",";
        json += "\"dryRunAlarm\":" + String(_dryRunTripped ? "true" : "false") + ",";
        json += "\"runDurationSec\":" + String(_pumpRunning ? (millis() - _pumpStartMs) / 1000 : 0) + ",";
        json += "\"maxRunTimeSec\":" + String(_maxRunTimeMs / 1000);
        json += "}";
        return json;
    }

    bool isSafeToUpdate() const override {
        // Critical safety: Unsafe to update or reboot while pump is actively running!
        return !_pumpRunning;
    }

    void prepareForUpdate() override {
        // Ensure relay is de-energized into fail-safe state before flash reboot
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

    void setRelay(bool on) {
        _pumpRunning = on;
        if (on) _pumpStartMs = millis();
        digitalWrite(_relayPin, _activeLow ? (!on ? HIGH : LOW) : (on ? HIGH : LOW));
    }
};
