#include "GitHubUpdater.h"
#include "ConfigStore.h"
#include "ModuleDetector.h"
#include "WiFiManager.h"
#include "CraftThingyCrypto.h"

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <WiFiClientSecure.h>
    #include <ESP8266HTTPClient.h>
#elif defined(ESP32)
    #include <WiFi.h>
    #include <WiFiClientSecure.h>
    #include <HTTPClient.h>
    #include <Update.h>
#endif

OtaUpdateStatus GitHubUpdater::_status = OTA_STATUS_IDLE;
String GitHubUpdater::_lastError = "";
uint32_t GitHubUpdater::_lastCheckMillis = 0;
bool GitHubUpdater::_forceCheckPending = false;

void GitHubUpdater::init() {
    _status = OTA_STATUS_IDLE;
    _lastError = "";
    _lastCheckMillis = 0;
    _forceCheckPending = false;
}

OtaUpdateStatus GitHubUpdater::getStatus() {
    return _status;
}

String GitHubUpdater::getStatusString() {
    switch (_status) {
        case OTA_STATUS_IDLE:        return "Idle";
        case OTA_STATUS_CHECKING:    return "Checking for Updates";
        case OTA_STATUS_NO_UPDATE:   return "Firmware Up to Date";
        case OTA_STATUS_DOWNLOADING: return "Downloading & Flashing OTA";
        case OTA_STATUS_SUCCESS:     return "OTA Success (Rebooting)";
        case OTA_STATUS_FAILED:      return "Update Failed: " + _lastError;
        default:                     return "Unknown";
    }
}

String GitHubUpdater::getLastError() {
    return _lastError;
}

uint32_t GitHubUpdater::getLastCheckTime() {
    return _lastCheckMillis;
}

void GitHubUpdater::triggerCheck() {
    _forceCheckPending = true;
}

void GitHubUpdater::loop() {
    if (!WiFiManager::isConnected()) return;

    DeviceConfig& cfg = ConfigStore::get();
    uint32_t intervalMs = cfg.otaIntervalSec * 1000UL;

    if (_forceCheckPending || (millis() - _lastCheckMillis >= intervalMs)) {
        _forceCheckPending = false;
        _lastCheckMillis = millis();
        checkForUpdates(true);
    }
}

bool GitHubUpdater::checkForUpdates(bool force) {
    if (!WiFiManager::isConnected()) {
        _lastError = "WiFi not connected";
        _status = OTA_STATUS_FAILED;
        return false;
    }

    _status = OTA_STATUS_CHECKING;
    DeviceConfig& cfg = ConfigStore::get();
    const HardwareFingerprint& fp = ModuleDetector::getFingerprint();

    String org = String(cfg.githubOrg);
    String coreRepo = String(cfg.coreRepo);
    String controllerRepo = String(cfg.controllerRepo);
    String activeRole = String(cfg.activeRole);

    // Tier 1: Check Core Framework Updater
    // If core repo update found, update core first
    bool coreUpdated = checkAndApplyRepoUpdate(org, coreRepo, CRAFT_CORE_VERSION, String(fp.archTag));
    if (coreUpdated) {
        return true; // Device will reboot
    }

    // Tier 2: Check Controller Application (e.g. water_pump, washing_machine)
    if (controllerRepo.length() > 0 && activeRole != "none") {
        String assetTarget = activeRole + "-" + String(fp.archTag);
        bool appUpdated = checkAndApplyRepoUpdate(org, controllerRepo, "1.0.0", assetTarget);
        if (appUpdated) {
            return true; // Device will reboot
        }
    }

    _status = OTA_STATUS_NO_UPDATE;
    return false;
}

bool GitHubUpdater::checkAndApplyRepoUpdate(const String& org, const String& repo, 
                                            const String& currentVersion, const String& assetTag) {
    if (org.length() == 0 || repo.length() == 0) return false;

    ReleaseInfo rel;
    if (!fetchLatestRelease(org, repo, assetTag, rel)) {
        return false;
    }

    if (!rel.found || rel.downloadUrl.length() == 0) {
        return false;
    }

    // Compare versions (clean 'v' prefix if present)
    String cleanRemote = rel.tagName;
    if (cleanRemote.startsWith("v") || cleanRemote.startsWith("V")) {
        cleanRemote = cleanRemote.substring(1);
    }

    String cleanLocal = currentVersion;
    if (cleanLocal.startsWith("v") || cleanLocal.startsWith("V")) {
        cleanLocal = cleanLocal.substring(1);
    }

    if (cleanRemote == cleanLocal) {
        return false; // Up to date
    }

    // Update found! Download and flash
    _status = OTA_STATUS_DOWNLOADING;
    return streamOtaFromUrl(rel.downloadUrl, rel.size);
}

// Memory-efficient GitHub API Stream Parser
bool GitHubUpdater::fetchLatestRelease(const String& org, const String& repo, 
                                       const String& assetTag, ReleaseInfo& outInfo) {
    outInfo.found = false;
    outInfo.size = 0;
    outInfo.downloadUrl = "";
    outInfo.tagName = "";

    WiFiClientSecure client;
#if defined(ESP8266)
    client.setBufferSizes(CRAFT_TLS_BUFFER_SIZE, CRAFT_TLS_BUFFER_SIZE);
    client.setInsecure();
#elif defined(ESP32)
    client.setInsecure();
#endif

    const char* host = "api.github.com";
    if (!client.connect(host, 443)) {
        _lastError = "Failed to connect to api.github.com";
        return false;
    }

    String path = "/repos/" + org + "/" + repo + "/releases/latest";
    DeviceConfig& cfg = ConfigStore::get();

    client.print("GET " + path + " HTTP/1.1\r\n");
    client.print("Host: " + String(host) + "\r\n");
    client.print("User-Agent: CraftThingy-ESP-Updater\r\n");
    client.print("Accept: application/vnd.github.v3+json\r\n");
    if (strlen(cfg.githubToken) > 0) {
        client.print("Authorization: token " + String(cfg.githubToken) + "\r\n");
    }
    client.print("Connection: close\r\n\r\n");

    // Wait for response headers
    uint32_t timeout = millis();
    while (client.connected() && !client.available()) {
        if (millis() - timeout > 10000) {
            client.stop();
            _lastError = "GitHub API timeout";
            return false;
        }
        delay(10);
    }

    // Read status line
    String statusLine = client.readStringUntil('\n');
    if (statusLine.indexOf("200") < 0) {
        _lastError = "GitHub API returned " + statusLine;
        client.stop();
        return false;
    }

    // Skip HTTP headers
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) {
            break;
        }
    }

    // Stream-parse JSON without loading full payload in RAM!
    // Search patterns: "tag_name": "...", "name": "...<assetTag>....bin", "browser_download_url": "..."
    String currentAssetName = "";
    String currentDownloadUrl = "";
    size_t currentAssetSize = 0;
    bool inAssets = false;

    String tokenBuffer = "";
    while (client.connected() || client.available()) {
        if (!client.available()) {
            delay(1);
            continue;
        }

        char c = (char)client.read();

        // Accumulate until delimiter
        if (c == ',' || c == '{' || c == '}' || c == '[' || c == ']') {
            tokenBuffer.trim();

            if (outInfo.tagName.length() == 0 && tokenBuffer.indexOf("\"tag_name\"") >= 0) {
                int colon = tokenBuffer.indexOf(':');
                if (colon >= 0) {
                    String tag = tokenBuffer.substring(colon + 1);
                    tag.replace("\"", "");
                    tag.trim();
                    outInfo.tagName = tag;
                }
            } else if (tokenBuffer.indexOf("\"assets\"") >= 0) {
                inAssets = true;
            } else if (inAssets && tokenBuffer.indexOf("\"name\"") >= 0) {
                int colon = tokenBuffer.indexOf(':');
                if (colon >= 0) {
                    currentAssetName = tokenBuffer.substring(colon + 1);
                    currentAssetName.replace("\"", "");
                    currentAssetName.trim();
                }
            } else if (inAssets && tokenBuffer.indexOf("\"size\"") >= 0) {
                int colon = tokenBuffer.indexOf(':');
                if (colon >= 0) {
                    currentAssetSize = tokenBuffer.substring(colon + 1).toInt();
                }
            } else if (inAssets && tokenBuffer.indexOf("\"browser_download_url\"") >= 0) {
                int colon = tokenBuffer.indexOf(':');
                if (colon >= 0) {
                    currentDownloadUrl = tokenBuffer.substring(colon + 1);
                    currentDownloadUrl.replace("\"", "");
                    currentDownloadUrl.trim();

                    // Check if current asset matches our board target tag!
                    if (currentAssetName.indexOf(assetTag) >= 0 || 
                        currentAssetName.endsWith(".bin")) {
                        outInfo.downloadUrl = currentDownloadUrl;
                        outInfo.size = currentAssetSize;
                        outInfo.found = true;
                        break; // Found matching asset!
                    }
                }
            }

            tokenBuffer = "";
        } else {
            if (tokenBuffer.length() < 256) {
                tokenBuffer += c;
            }
        }
    }

    client.stop();
    return outInfo.found;
}

// Stream OTA directly to Flash, handling HTTP 302 redirects to AWS S3
bool GitHubUpdater::streamOtaFromUrl(const String& url, size_t expectedSize) {
    String currentUrl = url;
    uint8_t redirectCount = 0;

    while (redirectCount < 4) {
        // Parse host and path from URL
        int protocolEnd = currentUrl.indexOf("://");
        if (protocolEnd < 0) return false;
        bool isHttps = currentUrl.startsWith("https");

        String hostAndPath = currentUrl.substring(protocolEnd + 3);
        int slashIndex = hostAndPath.indexOf('/');
        if (slashIndex < 0) return false;

        String host = hostAndPath.substring(0, slashIndex);
        String path = hostAndPath.substring(slashIndex);

        WiFiClientSecure client;
#if defined(ESP8266)
        client.setBufferSizes(CRAFT_TLS_BUFFER_SIZE, CRAFT_TLS_BUFFER_SIZE);
        client.setInsecure();
#elif defined(ESP32)
        client.setInsecure();
#endif

        uint16_t port = isHttps ? 443 : 80;
        if (!client.connect(host.c_str(), port)) {
            _lastError = "Connection failed to " + host;
            return false;
        }

        DeviceConfig& cfg = ConfigStore::get();
        client.print("GET " + path + " HTTP/1.1\r\n");
        client.print("Host: " + host + "\r\n");
        client.print("User-Agent: CraftThingy-ESP-Updater\r\n");
        if (strlen(cfg.githubToken) > 0 && host.indexOf("github.com") >= 0) {
            client.print("Authorization: token " + String(cfg.githubToken) + "\r\n");
        }
        client.print("Connection: close\r\n\r\n");

        // Wait for response headers
        uint32_t t = millis();
        while (client.connected() && !client.available()) {
            if (millis() - t > 12000) {
                client.stop();
                _lastError = "Stream timeout";
                return false;
            }
            delay(10);
        }

        String statusLine = client.readStringUntil('\n');
        size_t contentLength = expectedSize;
        String locationHeader = "";

        // Read headers
        while (client.connected()) {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) break; // End of headers

            if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
                contentLength = line.substring(15).toInt();
            } else if (line.startsWith("Location:") || line.startsWith("location:")) {
                locationHeader = line.substring(9);
                locationHeader.trim();
            }
        }

        // Handle 301 / 302 Redirect (Standard GitHub -> S3 behavior)
        if (statusLine.indexOf("301") >= 0 || statusLine.indexOf("302") >= 0) {
            client.stop();
            if (locationHeader.length() == 0) {
                _lastError = "Redirect without Location header";
                return false;
            }
            currentUrl = locationHeader;
            redirectCount++;
            continue;
        }

        if (statusLine.indexOf("200") < 0) {
            client.stop();
            _lastError = "Download failed: " + statusLine;
            return false;
        }

        // Begin Flash OTA stream
        if (contentLength == 0) {
            _lastError = "Zero content length for OTA";
            client.stop();
            return false;
        }

#if defined(ESP8266)
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (contentLength > maxSketchSpace) {
            _lastError = "Firmware exceeds ESP-01 1MB OTA partition (" + String(contentLength) + " > " + String(maxSketchSpace) + ")";
            client.stop();
            return false;
        }
        if (!Update.begin(contentLength, U_FLASH)) {
            _lastError = "Update.begin failed (" + String(Update.getError()) + ")";
            client.stop();
            return false;
        }
#elif defined(ESP32)
        if (!Update.begin(contentLength, U_FLASH)) {
            _lastError = "Update.begin failed (" + String(Update.getError()) + ")";
            client.stop();
            return false;
        }
#endif

        // Stream bytes directly to Flash
        uint8_t buffer[CRAFT_STREAM_CHUNK_SIZE];
        size_t written = 0;
        uint32_t lastDataTime = millis();

        while (client.connected() && written < contentLength) {
            size_t available = client.available();
            if (available > 0) {
                size_t toRead = available < sizeof(buffer) ? available : sizeof(buffer);
                if (written + toRead > contentLength) {
                    toRead = contentLength - written;
                }

                size_t bytesRead = client.readBytes(buffer, toRead);
                if (bytesRead > 0) {
                    size_t bytesWritten = Update.write(buffer, bytesRead);
                    if (bytesWritten != bytesRead) {
                        _lastError = "Flash write error";
                        Update.end(false);
                        client.stop();
                        return false;
                    }
                    written += bytesWritten;
                    lastDataTime = millis();
                }
            } else {
                if (millis() - lastDataTime > 10000) {
                    _lastError = "OTA stream stall timeout";
                    Update.end(false);
                    client.stop();
                    return false;
                }
                delay(2);
                yield();
            }
        }

        client.stop();

        if (written == contentLength && Update.end(true)) {
            _status = OTA_STATUS_SUCCESS;
            delay(500);
            ESP.restart();
            return true;
        } else {
            _lastError = "OTA verification failed (" + String(Update.getError()) + ")";
            _status = OTA_STATUS_FAILED;
            return false;
        }
    }

    _lastError = "Too many HTTP redirects";
    return false;
}
