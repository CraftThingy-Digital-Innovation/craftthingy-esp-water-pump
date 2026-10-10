#pragma once

#include <Arduino.h>
#include "CraftThingyConfig.h"

struct DeviceConfig {
    char wifiSsid[33];
    char wifiPassword[65];
    char adminPasswordHash[65];
    char masterSecretKey[65];
    char currentRollingKey[65];
    uint32_t rollingSequence;
    char githubOrg[64];
    char coreRepo[64];
    char controllerRepo[64];
    char activeRole[32];
    char githubToken[64];
    uint32_t otaIntervalSec;
    bool configured;
};

class ConfigStore {
public:
    static bool init();
    static bool load();
    static bool save();
    static void resetToDefaults();

    static DeviceConfig& get();

    // Advance the rolling key sequence and update persistent storage
    static bool rotateRollingKey();

    // Update GitHub target configuration
    static bool setGitHubConfig(const String& org, const String& coreRepo, 
                                const String& controllerRepo, const String& token);

    // Update WiFi configuration
    static bool setWiFiConfig(const String& ssid, const String& pass);

    // Update Active Role
    static bool setActiveRole(const String& role);

    // Check if device is configured
    static bool isConfigured();

private:
    static DeviceConfig _config;
    static void populateDefaults();
};
