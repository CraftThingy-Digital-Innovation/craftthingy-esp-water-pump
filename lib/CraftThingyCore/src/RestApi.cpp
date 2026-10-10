#include "RestApi.h"
#include "ConfigStore.h"
#include "WiFiManager.h"
#include "GitHubUpdater.h"
#include "ModuleDetector.h"
#include "CraftThingyCrypto.h"

ApiServerType RestApi::_server(CRAFT_WEB_PORT);

void RestApi::init() {
    // Collect security headers for HMAC and Bearer authentication
    const char* headerKeys[] = {"X-Signature", "X-Timestamp", "X-Nonce", "X-Sequence", "Authorization"};
    _server.collectHeaders(headerKeys, 5);

    setupRoutes();
    _server.begin();
}

void RestApi::loop() {
    _server.handleClient();
}

void RestApi::sendJson(int code, const String& json) {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-Signature, X-Timestamp, X-Nonce, X-Sequence, Authorization");
    _server.send(code, "application/json", json);
}

void RestApi::sendError(int code, const String& error) {
    sendJson(code, "{\"status\":\"error\",\"message\":\"" + error + "\"}");
}

void RestApi::sendSuccess(const String& msg) {
    sendJson(200, "{\"status\":\"success\",\"message\":\"" + msg + "\"}");
}

bool RestApi::isRequestAuthorized(ApiServerType& server, String& outMethod) {
    DeviceConfig& cfg = ConfigStore::get();

    String sig = server.header("X-Signature");
    String timestamp = server.header("X-Timestamp");
    String nonce = server.header("X-Nonce");
    String seqStr = server.header("X-Sequence");
    String authHeader = server.header("Authorization");

    // Tier 1: HMAC-SHA256 Rolling Key Verification (Highest Security)
    if (sig.length() == 64 && nonce.length() > 0) {
        uint32_t seq = seqStr.toInt();

        // Anti-replay check
        if (!CraftThingyCrypto::checkAndRecordNonce(nonce, seq)) {
            outMethod = "rejected_replay";
            return false;
        }

        String method = server.method() == HTTP_POST ? "POST" : 
                       (server.method() == HTTP_GET ? "GET" : "PUT");
        String path = server.uri();
        String body = server.arg("plain");

        // Verify with current rolling key
        if (CraftThingyCrypto::verifyRequestSignature(cfg.currentRollingKey, method, path, timestamp, nonce, body, sig)) {
            outMethod = "hmac_rolling_key";
            return true;
        }

        // Fallback: Verify with master secret key
        if (CraftThingyCrypto::verifyRequestSignature(cfg.masterSecretKey, method, path, timestamp, nonce, body, sig)) {
            outMethod = "hmac_master_key";
            return true;
        }

        outMethod = "invalid_signature";
        return false;
    }

    // Tier 2: Bearer Token Authentication
    if (authHeader.startsWith("Bearer ") || authHeader.startsWith("bearer ")) {
        String token = authHeader.substring(7);
        token.trim();
        if (CraftThingyCrypto::constantTimeEquals(token, String(cfg.currentRollingKey)) ||
            CraftThingyCrypto::constantTimeEquals(token, String(cfg.masterSecretKey))) {
            outMethod = "bearer_token";
            return true;
        }
    }

    // Tier 3: Admin Basic Auth
    // Default admin / SHA-256 password comparison or HTTP basic auth
    if (server.authenticate("admin", cfg.adminPasswordHash)) {
        outMethod = "basic_auth";
        return true;
    }

    outMethod = "unauthorized";
    return false;
}

void RestApi::setupRoutes() {
    _server.on("/api/status", HTTP_GET, handleStatus);
    _server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
    _server.on("/api/wifi", HTTP_POST, handleWifiConfig);
    _server.on("/api/github", HTTP_POST, handleGitHubConfig);
    _server.on("/api/update", HTTP_POST, handleTriggerUpdate);
    _server.on("/api/key/rotate", HTTP_POST, handleRotateKey);
    _server.on("/api/reboot", HTTP_POST, handleReboot);

    // OPTIONS pre-flight for cross-origin tools
    _server.onNotFound(handleNotFound);
}

void RestApi::handleStatus() {
    DeviceConfig& cfg = ConfigStore::get();
    const HardwareFingerprint& fp = ModuleDetector::getFingerprint();

    String json = "{";
    json += "\"status\":\"online\",";
    json += "\"coreVersion\":\"" + String(CRAFT_CORE_VERSION) + "\",";
    json += "\"boardTarget\":\"" + String(fp.archTag) + "\",";
    json += "\"chip\":\"" + String(fp.chipName) + "\",";
    json += "\"flashBytes\":" + String(fp.flashSizeBytes) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"mac\":\"" + fp.macAddress + "\",";
    json += "\"wifiState\":\"" + String(WiFiManager::isConnected() ? "connected" : "ap_mode") + "\",";
    json += "\"ip\":\"" + (WiFiManager::isConnected() ? WiFiManager::getIPAddress() : WiFiManager::getAPIPAddress()) + "\",";
    json += "\"activeRole\":\"" + String(cfg.activeRole) + "\",";
    json += "\"githubOrg\":\"" + String(cfg.githubOrg) + "\",";
    json += "\"coreRepo\":\"" + String(cfg.coreRepo) + "\",";
    json += "\"controllerRepo\":\"" + String(cfg.controllerRepo) + "\",";
    json += "\"rollingSequence\":" + String(cfg.rollingSequence) + ",";
    json += "\"currentRollingKeyPrefix\":\"" + String(cfg.currentRollingKey).substring(0, 8) + "...\",";
    json += "\"otaStatus\":\"" + GitHubUpdater::getStatusString() + "\"";
    json += "}";

    sendJson(200, json);
}

void RestApi::handleWifiScan() {
    String scanResult = WiFiManager::scanNetworksJson();
    sendJson(200, scanResult);
}

void RestApi::handleWifiConfig() {
    String authMethod;
    if (!isRequestAuthorized(_server, authMethod)) {
        sendError(401, "Unauthorized: Valid HMAC rolling key signature or token required (" + authMethod + ")");
        return;
    }

    String ssid = _server.hasArg("ssid") ? _server.arg("ssid") : "";
    String pass = _server.hasArg("pass") ? _server.arg("pass") : "";

    // Also support JSON body if plain arg passed
    if (ssid.length() == 0 && _server.hasArg("plain")) {
        String body = _server.arg("plain");
        int ssidIdx = body.indexOf("\"ssid\"");
        if (ssidIdx >= 0) {
            int start = body.indexOf(':', ssidIdx) + 1;
            int q1 = body.indexOf('"', start);
            int q2 = body.indexOf('"', q1 + 1);
            if (q1 >= 0 && q2 > q1) ssid = body.substring(q1 + 1, q2);
        }
        int passIdx = body.indexOf("\"pass\"");
        if (passIdx >= 0) {
            int start = body.indexOf(':', passIdx) + 1;
            int q1 = body.indexOf('"', start);
            int q2 = body.indexOf('"', q1 + 1);
            if (q1 >= 0 && q2 > q1) pass = body.substring(q1 + 1, q2);
        }
    }

    if (ssid.length() == 0) {
        sendError(400, "Missing 'ssid' parameter");
        return;
    }

    ConfigStore::setWiFiConfig(ssid, pass);
    sendSuccess("WiFi credentials saved. Reconnecting in background...");

    // Schedule connection attempt
    WiFiManager::connectSTA(ssid.c_str(), pass.c_str(), CRAFT_WIFI_MAX_RETRIES);
}

void RestApi::handleGitHubConfig() {
    String authMethod;
    if (!isRequestAuthorized(_server, authMethod)) {
        sendError(401, "Unauthorized: Valid HMAC rolling key signature or token required (" + authMethod + ")");
        return;
    }

    String org = _server.hasArg("org") ? _server.arg("org") : "";
    String coreRepo = _server.hasArg("core_repo") ? _server.arg("core_repo") : "";
    String ctrlRepo = _server.hasArg("controller_repo") ? _server.arg("controller_repo") : "";
    String role = _server.hasArg("role") ? _server.arg("role") : "";
    String token = _server.hasArg("token") ? _server.arg("token") : "";

    // Parse JSON body if provided
    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        auto extractJson = [&](const String& key) -> String {
            int idx = body.indexOf("\"" + key + "\"");
            if (idx < 0) return "";
            int start = body.indexOf(':', idx) + 1;
            int q1 = body.indexOf('"', start);
            int q2 = body.indexOf('"', q1 + 1);
            if (q1 >= 0 && q2 > q1) return body.substring(q1 + 1, q2);
            return "";
        };

        if (org.length() == 0) org = extractJson("org");
        if (coreRepo.length() == 0) coreRepo = extractJson("core_repo");
        if (ctrlRepo.length() == 0) ctrlRepo = extractJson("controller_repo");
        if (role.length() == 0) role = extractJson("role");
        if (token.length() == 0) token = extractJson("token");
    }

    ConfigStore::setGitHubConfig(org, coreRepo, ctrlRepo, token);
    if (role.length() > 0) {
        ConfigStore::setActiveRole(role);
    }

    // Automatically rotate rolling key after authenticated config update for forward secrecy!
    ConfigStore::rotateRollingKey();

    DeviceConfig& cfg = ConfigStore::get();
    String resp = "{\"status\":\"success\",";
    resp += "\"message\":\"GitHub configuration updated and rolling key ratcheted\",";
    resp += "\"newRollingSequence\":" + String(cfg.rollingSequence) + ",";
    resp += "\"newRollingKey\":\"" + String(cfg.currentRollingKey) + "\"}";

    sendJson(200, resp);
}

void RestApi::handleTriggerUpdate() {
    String authMethod;
    if (!isRequestAuthorized(_server, authMethod)) {
        sendError(401, "Unauthorized: Valid HMAC rolling key signature or token required");
        return;
    }

    GitHubUpdater::triggerCheck();
    sendSuccess("GitHub OTA update check triggered");
}

void RestApi::handleRotateKey() {
    String authMethod;
    if (!isRequestAuthorized(_server, authMethod)) {
        sendError(401, "Unauthorized: Valid HMAC rolling key signature or token required");
        return;
    }

    if (ConfigStore::rotateRollingKey()) {
        DeviceConfig& cfg = ConfigStore::get();
        String resp = "{\"status\":\"success\",";
        resp += "\"message\":\"Rolling key advanced to next ratchet sequence\",";
        resp += "\"newRollingSequence\":" + String(cfg.rollingSequence) + ",";
        resp += "\"newRollingKey\":\"" + String(cfg.currentRollingKey) + "\"}";
        sendJson(200, resp);
    } else {
        sendError(500, "Failed to rotate rolling key");
    }
}

void RestApi::handleReboot() {
    String authMethod;
    if (!isRequestAuthorized(_server, authMethod)) {
        sendError(401, "Unauthorized: Valid HMAC rolling key signature or token required");
        return;
    }

    sendSuccess("Device restarting...");
    delay(500);
    ESP.restart();
}

void RestApi::handleNotFound() {
    if (_server.method() == HTTP_OPTIONS) {
        _server.sendHeader("Access-Control-Allow-Origin", "*");
        _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        _server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-Signature, X-Timestamp, X-Nonce, X-Sequence, Authorization");
        _server.send(204);
        return;
    }

    sendError(404, "Endpoint not found: " + _server.uri());
}
