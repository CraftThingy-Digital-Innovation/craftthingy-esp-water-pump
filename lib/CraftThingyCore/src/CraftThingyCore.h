#pragma once

#include <Arduino.h>
#include "CraftThingyConfig.h"
#include "ModuleDetector.h"
#include "ConfigStore.h"
#include "CraftThingyCrypto.h"
#include "WiFiManager.h"
#include "GitHubUpdater.h"
#include "RestApi.h"
#include "DeviceController.h"

class CraftThingyCoreClass {
public:
    CraftThingyCoreClass();

    // Initialize all core subsystems and start WiFi / AP
    void begin(IDeviceController* activeController = nullptr);

    // Call in the main sketch loop()
    void loop();

    // Attach or swap active hardware controller dynamically
    void setController(IDeviceController* controller);
    IDeviceController* getController();

    // Quick status
    bool isWiFiConnected();
    bool isAPMode();

private:
    IDeviceController* _controller;
};

extern CraftThingyCoreClass CraftThingyCore;
