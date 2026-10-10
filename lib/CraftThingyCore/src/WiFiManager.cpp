#include "WiFiManager.h"
#include "ConfigStore.h"

WiFiState WiFiManager::_state = WIFI_STATE_IDLE;
DNSServer WiFiManager::_dnsServer;
String WiFiManager::_apSSID = "CraftThingy-Setup";
uint32_t WiFiManager::_lastReconnectAttempt = 0;
uint8_t WiFiManager::_retryCount = 0;
bool WiFiManager::_dnsActive = false;

void WiFiManager::init() {
    String mac = getMacAddress();
    mac.replace(":", "");
    String suffix = mac.substring(mac.length() - 4);
    suffix.toUpperCase();
    _apSSID = "CraftThingy-" + suffix;

    WiFi.persistent(false);
    WiFi.disconnect(true);
    _state = WIFI_STATE_IDLE;
}

bool WiFiManager::connectSTA(const char* ssid, const char* password, uint8_t maxRetries) {
    if (!ssid || strlen(ssid) == 0) {
        startAPPortal();
        return false;
    }

    _state = WIFI_STATE_CONNECTING;
    WiFi.mode(WIFI_STA);

    for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
        WiFi.begin(ssid, password);

        // Wait up to 12 seconds per attempt
        uint32_t startMs = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 12000) {
            delay(200);
            yield();
        }

        if (WiFi.status() == WL_CONNECTED) {
            _state = WIFI_STATE_CONNECTED;
            _retryCount = 0;
            if (_dnsActive) {
                _dnsServer.stop();
                _dnsActive = false;
            }
            return true;
        }

        delay(CRAFT_WIFI_RETRY_DELAY_MS);
    }

    _state = WIFI_STATE_FAILED;
    startAPPortal();
    return false;
}

void WiFiManager::startAPPortal() {
    _state = WIFI_STATE_AP_PORTAL;

    WiFi.mode(WIFI_AP_STA);
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);

    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(_apSSID.c_str(), nullptr, CRAFT_AP_CHANNEL);

    if (!_dnsActive) {
        _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        _dnsServer.start(CRAFT_DNS_PORT, "*", apIP);
        _dnsActive = true;
    }
}

bool WiFiManager::retryStation() {
    DeviceConfig& cfg = ConfigStore::get();
    if (strlen(cfg.wifiSsid) > 0) {
        return connectSTA(cfg.wifiSsid, cfg.wifiPassword, 3);
    }
    return false;
}

void WiFiManager::loop() {
    if (_state == WIFI_STATE_AP_PORTAL) {
        if (_dnsActive) {
            _dnsServer.processNextRequest();
        }

        // Periodic background attempt every 60 seconds if configured
        if (ConfigStore::isConfigured() && (millis() - _lastReconnectAttempt > 60000)) {
            _lastReconnectAttempt = millis();
            DeviceConfig& cfg = ConfigStore::get();
            if (WiFi.status() != WL_CONNECTED) {
                WiFi.begin(cfg.wifiSsid, cfg.wifiPassword);
                uint32_t testStart = millis();
                while (WiFi.status() != WL_CONNECTED && (millis() - testStart) < 4000) {
                    delay(100);
                    yield();
                }
                if (WiFi.status() == WL_CONNECTED) {
                    WiFi.mode(WIFI_STA);
                    if (_dnsActive) {
                        _dnsServer.stop();
                        _dnsActive = false;
                    }
                    _state = WIFI_STATE_CONNECTED;
                }
            }
        }
    } else if (_state == WIFI_STATE_CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            _state = WIFI_STATE_CONNECTING;
            DeviceConfig& cfg = ConfigStore::get();
            if (!connectSTA(cfg.wifiSsid, cfg.wifiPassword, CRAFT_WIFI_MAX_RETRIES)) {
                startAPPortal();
            }
        }
    }
}

WiFiState WiFiManager::getState() {
    return _state;
}

bool WiFiManager::isConnected() {
    return (_state == WIFI_STATE_CONNECTED) && (WiFi.status() == WL_CONNECTED);
}

bool WiFiManager::isAPMode() {
    return _state == WIFI_STATE_AP_PORTAL;
}

String WiFiManager::getIPAddress() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

String WiFiManager::getAPIPAddress() {
    return WiFi.softAPIP().toString();
}

String WiFiManager::getMacAddress() {
    return WiFi.macAddress();
}

String WiFiManager::getApSSID() {
    return _apSSID;
}

String WiFiManager::scanNetworksJson() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
#if defined(ESP8266)
        json += "\"enc\":" + String(WiFi.encryptionType(i) != ENC_TYPE_NONE ? "true" : "false") + "}";
#elif defined(ESP32)
        json += "\"enc\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
#endif
    }
    json += "]";
    return json;
}
