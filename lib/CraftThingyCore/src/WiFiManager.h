#pragma once

#include <Arduino.h>
#include "CraftThingyConfig.h"

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif
#include <DNSServer.h>

enum WiFiState {
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
    WIFI_STATE_AP_PORTAL
};

class WiFiManager {
public:
    static void init();
    static void loop();

    // Trigger station connection with retries
    static bool connectSTA(const char* ssid, const char* password, uint8_t maxRetries = CRAFT_WIFI_MAX_RETRIES);

    // Switch to SoftAP Captive Portal mode
    static void startAPPortal();

    // Stop AP mode and attempt reconnecting
    static bool retryStation();

    // Check current state
    static WiFiState getState();
    static bool isConnected();
    static bool isAPMode();

    static String getIPAddress();
    static String getAPIPAddress();
    static String getMacAddress();
    static String getApSSID();

    // Scan nearby networks and return JSON list
    static String scanNetworksJson();

private:
    static WiFiState _state;
    static DNSServer _dnsServer;
    static String _apSSID;
    static uint32_t _lastReconnectAttempt;
    static uint8_t _retryCount;
    static bool _dnsActive;
};
