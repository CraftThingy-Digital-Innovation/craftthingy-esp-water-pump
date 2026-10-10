#include "ConfigStore.h"
#include "CraftThingyCrypto.h"

#if defined(ESP8266)
    #include <EEPROM.h>
    #define EEPROM_SIZE 1024
    #define EEPROM_MAGIC 0x43524654 // 'CRFT'
    struct EepromContainer {
        uint32_t magic;
        uint32_t checksum;
        DeviceConfig config;
    };
#elif defined(ESP32)
    #include <Preferences.h>
    static Preferences prefs;
#endif

DeviceConfig ConfigStore::_config;

void ConfigStore::populateDefaults() {
    memset(&_config, 0, sizeof(DeviceConfig));
    strncpy(_config.wifiSsid, "", sizeof(_config.wifiSsid) - 1);
    strncpy(_config.wifiPassword, "", sizeof(_config.wifiPassword) - 1);
    
    // Default admin password hash for "admin" (SHA256: 8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918)
    strncpy(_config.adminPasswordHash, "8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918", sizeof(_config.adminPasswordHash) - 1);

    // Initial default master secret key (64 hex characters)
    String defaultSecret = "a1b2c3d4e5f60718293a4b5c6d7e8f90112233445566778899aabbccddeeff00";
    strncpy(_config.masterSecretKey, defaultSecret.c_str(), sizeof(_config.masterSecretKey) - 1);
    strncpy(_config.currentRollingKey, defaultSecret.c_str(), sizeof(_config.currentRollingKey) - 1);
    _config.rollingSequence = 1;

    strncpy(_config.githubOrg, CRAFT_DEFAULT_ORG, sizeof(_config.githubOrg) - 1);
    strncpy(_config.coreRepo, CRAFT_DEFAULT_CORE_REPO, sizeof(_config.coreRepo) - 1);
    strncpy(_config.controllerRepo, CRAFT_DEFAULT_CONTROLLER_REPO, sizeof(_config.controllerRepo) - 1);
    strncpy(_config.activeRole, CRAFT_DEFAULT_ROLE, sizeof(_config.activeRole) - 1);
    strncpy(_config.githubToken, "", sizeof(_config.githubToken) - 1);
    _config.otaIntervalSec = CRAFT_OTA_CHECK_INTERVAL_S;
    _config.configured = false;
}

bool ConfigStore::init() {
#if defined(ESP8266)
    EEPROM.begin(EEPROM_SIZE);
#endif
    return load();
}

bool ConfigStore::load() {
#if defined(ESP8266)
    EepromContainer container;
    EEPROM.get(0, container);

    if (container.magic != EEPROM_MAGIC) {
        populateDefaults();
        return save();
    }

    // Verify simple checksum
    uint32_t calcCheck = 0;
    const uint8_t* p = (const uint8_t*)&container.config;
    for (size_t i = 0; i < sizeof(DeviceConfig); ++i) {
        calcCheck = (calcCheck * 31) + p[i];
    }

    if (calcCheck != container.checksum) {
        populateDefaults();
        return save();
    }

    _config = container.config;
    return true;

#elif defined(ESP32)
    if (!prefs.begin("craft_cfg", true)) {
        populateDefaults();
        return save();
    }

    if (!prefs.isKey("cfg_init")) {
        prefs.end();
        populateDefaults();
        return save();
    }

    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    String admin = prefs.getString("admin", "");
    String master = prefs.getString("master", "");
    String rolling = prefs.getString("rolling", "");
    uint32_t seq = prefs.getUInt("seq", 1);
    String org = prefs.getString("org", CRAFT_DEFAULT_ORG);
    String core = prefs.getString("core", CRAFT_DEFAULT_CORE_REPO);
    String ctrl = prefs.getString("ctrl", CRAFT_DEFAULT_CONTROLLER_REPO);
    String role = prefs.getString("role", CRAFT_DEFAULT_ROLE);
    String token = prefs.getString("token", "");
    uint32_t interval = prefs.getUInt("interval", CRAFT_OTA_CHECK_INTERVAL_S);
    bool conf = prefs.getBool("configured", false);
    prefs.end();

    strncpy(_config.wifiSsid, ssid.c_str(), sizeof(_config.wifiSsid) - 1);
    strncpy(_config.wifiPassword, pass.c_str(), sizeof(_config.wifiPassword) - 1);
    strncpy(_config.adminPasswordHash, admin.c_str(), sizeof(_config.adminPasswordHash) - 1);
    strncpy(_config.masterSecretKey, master.c_str(), sizeof(_config.masterSecretKey) - 1);
    strncpy(_config.currentRollingKey, rolling.c_str(), sizeof(_config.currentRollingKey) - 1);
    _config.rollingSequence = seq;
    strncpy(_config.githubOrg, org.c_str(), sizeof(_config.githubOrg) - 1);
    strncpy(_config.coreRepo, core.c_str(), sizeof(_config.coreRepo) - 1);
    strncpy(_config.controllerRepo, ctrl.c_str(), sizeof(_config.controllerRepo) - 1);
    strncpy(_config.activeRole, role.c_str(), sizeof(_config.activeRole) - 1);
    strncpy(_config.githubToken, token.c_str(), sizeof(_config.githubToken) - 1);
    _config.otaIntervalSec = interval;
    _config.configured = conf;

    return true;
#endif
}

bool ConfigStore::save() {
#if defined(ESP8266)
    EepromContainer container;
    container.magic = EEPROM_MAGIC;
    container.config = _config;

    uint32_t calcCheck = 0;
    const uint8_t* p = (const uint8_t*)&container.config;
    for (size_t i = 0; i < sizeof(DeviceConfig); ++i) {
        calcCheck = (calcCheck * 31) + p[i];
    }
    container.checksum = calcCheck;

    EEPROM.put(0, container);
    return EEPROM.commit();

#elif defined(ESP32)
    if (!prefs.begin("craft_cfg", false)) {
        return false;
    }
    prefs.putBool("cfg_init", true);
    prefs.putString("ssid", _config.wifiSsid);
    prefs.putString("pass", _config.wifiPassword);
    prefs.putString("admin", _config.adminPasswordHash);
    prefs.putString("master", _config.masterSecretKey);
    prefs.putString("rolling", _config.currentRollingKey);
    prefs.putUInt("seq", _config.rollingSequence);
    prefs.putString("org", _config.githubOrg);
    prefs.putString("core", _config.coreRepo);
    prefs.putString("ctrl", _config.controllerRepo);
    prefs.putString("role", _config.activeRole);
    prefs.putString("token", _config.githubToken);
    prefs.putUInt("interval", _config.otaIntervalSec);
    prefs.putBool("configured", _config.configured);
    prefs.end();
    return true;
#endif
}

void ConfigStore::resetToDefaults() {
    populateDefaults();
    save();
}

DeviceConfig& ConfigStore::get() {
    return _config;
}

bool ConfigStore::rotateRollingKey() {
    String currentKey = String(_config.currentRollingKey);
    _config.rollingSequence++;

    String nextKey;
    if (!CraftThingyCrypto::deriveNextRollingKey(currentKey, _config.rollingSequence, nextKey)) {
        return false;
    }

    strncpy(_config.currentRollingKey, nextKey.c_str(), sizeof(_config.currentRollingKey) - 1);
    return save();
}

bool ConfigStore::setGitHubConfig(const String& org, const String& coreRepo, 
                                  const String& controllerRepo, const String& token) {
    if (org.length() > 0) strncpy(_config.githubOrg, org.c_str(), sizeof(_config.githubOrg) - 1);
    if (coreRepo.length() > 0) strncpy(_config.coreRepo, coreRepo.c_str(), sizeof(_config.coreRepo) - 1);
    if (controllerRepo.length() > 0) strncpy(_config.controllerRepo, controllerRepo.c_str(), sizeof(_config.controllerRepo) - 1);
    if (token.length() > 0) strncpy(_config.githubToken, token.c_str(), sizeof(_config.githubToken) - 1);
    return save();
}

bool ConfigStore::setWiFiConfig(const String& ssid, const String& pass) {
    strncpy(_config.wifiSsid, ssid.c_str(), sizeof(_config.wifiSsid) - 1);
    strncpy(_config.wifiPassword, pass.c_str(), sizeof(_config.wifiPassword) - 1);
    _config.configured = true;
    return save();
}

bool ConfigStore::setActiveRole(const String& role) {
    strncpy(_config.activeRole, role.c_str(), sizeof(_config.activeRole) - 1);
    return save();
}

bool ConfigStore::isConfigured() {
    return _config.configured && (strlen(_config.wifiSsid) > 0);
}
