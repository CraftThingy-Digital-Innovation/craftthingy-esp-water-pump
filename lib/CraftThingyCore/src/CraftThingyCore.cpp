#include "CraftThingyCore.h"

CraftThingyCoreClass CraftThingyCore;

CraftThingyCoreClass::CraftThingyCoreClass() : _controller(nullptr) {}

void CraftThingyCoreClass::begin(IDeviceController* activeController) {
    _controller = activeController;

    // 1. Detect hardware, chip, flash size, and peripherals
    ModuleDetector::init();

    // 2. Initialize native crypto and anti-replay cache
    CraftThingyCrypto::init();

    // 3. Load persistent configuration from NVS / EEPROM
    ConfigStore::init();
    DeviceConfig& cfg = ConfigStore::get();

    // 4. Initialize WiFi state machine & SoftAP fallback
    WiFiManager::init();

    // 5. Initialize lightweight REST API server
    RestApi::init();

    // 6. Initialize GitHub OTA Updater engine
    GitHubUpdater::init();

    // 7. If controller attached, initialize its hardware pins & peripherals
    if (_controller != nullptr) {
        _controller->setup();
    }

    // 8. Connect to saved WiFi or start SoftAP
    if (ConfigStore::isConfigured() && strlen(cfg.wifiSsid) > 0) {
        WiFiManager::connectSTA(cfg.wifiSsid, cfg.wifiPassword, CRAFT_WIFI_MAX_RETRIES);
    } else {
        WiFiManager::startAPPortal();
    }
}

void CraftThingyCoreClass::loop() {
    // Process WiFi connection state and failover
    WiFiManager::loop();

    // Process incoming REST API client requests
    RestApi::loop();

    // Process GitHub OTA check timer and streaming download
    GitHubUpdater::loop();

    // Process active application controller loop (pump, washing machine, etc.)
    if (_controller != nullptr) {
        _controller->loop();
    }
}

void CraftThingyCoreClass::setController(IDeviceController* controller) {
    _controller = controller;
    if (_controller != nullptr) {
        _controller->setup();
    }
}

IDeviceController* CraftThingyCoreClass::getController() {
    return _controller;
}

bool CraftThingyCoreClass::isWiFiConnected() {
    return WiFiManager::isConnected();
}

bool CraftThingyCoreClass::isAPMode() {
    return WiFiManager::isAPMode();
}
