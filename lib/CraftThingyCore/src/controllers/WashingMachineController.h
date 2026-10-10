#pragma once

#include <Arduino.h>
#include "../DeviceController.h"

enum WasherState {
    WASHER_IDLE,
    WASHER_FILLING,
    WASHER_WASHING,
    WASHER_DRAINING,
    WASHER_RINSING,
    WASHER_SPINNING,
    WASHER_COMPLETED,
    WASHER_ERROR
};

class WashingMachineController : public IDeviceController {
public:
    WashingMachineController(uint8_t valvePin = 12, uint8_t motorPin = 13, uint8_t drainPin = 14)
        : _valvePin(valvePin), _motorPin(motorPin), _drainPin(drainPin),
          _state(WASHER_IDLE), _stateStartMs(0), _cycleTimerMs(0) {}

    void setup() override {
        pinMode(_valvePin, OUTPUT);
        pinMode(_motorPin, OUTPUT);
        pinMode(_drainPin, OUTPUT);
        allRelaysOff();
    }

    void loop() override {
        uint32_t elapsed = millis() - _stateStartMs;

        switch (_state) {
            case WASHER_IDLE:
                break;

            case WASHER_FILLING:
                // Simulate fill time (e.g. 10 seconds or until level sensor)
                if (elapsed >= 10000) {
                    digitalWrite(_valvePin, LOW);
                    transitionTo(WASHER_WASHING);
                }
                break;

            case WASHER_WASHING:
                // Agitate motor for 15 seconds
                digitalWrite(_motorPin, ((millis() / 1000) % 2 == 0) ? HIGH : LOW);
                if (elapsed >= 15000) {
                    digitalWrite(_motorPin, LOW);
                    transitionTo(WASHER_DRAINING);
                }
                break;

            case WASHER_DRAINING:
                digitalWrite(_drainPin, HIGH);
                if (elapsed >= 8000) {
                    digitalWrite(_drainPin, LOW);
                    transitionTo(WASHER_SPINNING);
                }
                break;

            case WASHER_SPINNING:
                digitalWrite(_drainPin, HIGH);
                digitalWrite(_motorPin, HIGH);
                if (elapsed >= 12000) {
                    allRelaysOff();
                    transitionTo(WASHER_COMPLETED);
                }
                break;

            case WASHER_COMPLETED:
                // Complete cycle
                break;

            case WASHER_ERROR:
                allRelaysOff();
                break;
        }
    }

    const char* getRoleName() const override {
        return "washing_machine";
    }

    const char* getAppVersion() const override {
        return "1.0.0";
    }

    bool handleCommand(const String& cmd, const String& payload, String& response) override {
        if (cmd == "start" && _state == WASHER_IDLE) {
            transitionTo(WASHER_FILLING);
            digitalWrite(_valvePin, HIGH);
            response = "Wash cycle STARTED";
            return true;
        } else if (cmd == "pause" || cmd == "stop") {
            allRelaysOff();
            transitionTo(WASHER_IDLE);
            response = "Wash cycle STOPPED";
            return true;
        } else if (cmd == "drain") {
            allRelaysOff();
            digitalWrite(_drainPin, HIGH);
            transitionTo(WASHER_DRAINING);
            response = "Draining water";
            return true;
        }
        return false;
    }

    String getStatusJson() override {
        String json = "{";
        json += "\"state\":\"" + getStateString() + "\",";
        json += "\"elapsedSeconds\":" + String((millis() - _stateStartMs) / 1000) + ",";
        json += "\"valveActive\":" + String(digitalRead(_valvePin) == HIGH ? "true" : "false") + ",";
        json += "\"motorActive\":" + String(digitalRead(_motorPin) == HIGH ? "true" : "false") + ",";
        json += "\"drainActive\":" + String(digitalRead(_drainPin) == HIGH ? "true" : "false");
        json += "}";
        return json;
    }

private:
    uint8_t _valvePin;
    uint8_t _motorPin;
    uint8_t _drainPin;
    WasherState _state;
    uint32_t _stateStartMs;
    uint32_t _cycleTimerMs;

    void transitionTo(WasherState newState) {
        _state = newState;
        _stateStartMs = millis();
    }

    void allRelaysOff() {
        digitalWrite(_valvePin, LOW);
        digitalWrite(_motorPin, LOW);
        digitalWrite(_drainPin, LOW);
    }

    String getStateString() {
        switch (_state) {
            case WASHER_IDLE:      return "idle";
            case WASHER_FILLING:   return "filling";
            case WASHER_WASHING:   return "washing";
            case WASHER_DRAINING:  return "draining";
            case WASHER_RINSING:   return "rinsing";
            case WASHER_SPINNING:  return "spinning";
            case WASHER_COMPLETED: return "completed";
            case WASHER_ERROR:     return "error";
            default:               return "unknown";
        }
    }
};
