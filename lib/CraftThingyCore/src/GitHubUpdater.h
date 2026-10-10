#pragma once

#include <Arduino.h>
#include "CraftThingyConfig.h"

enum OtaUpdateStatus {
    OTA_STATUS_IDLE,
    OTA_STATUS_CHECKING,
    OTA_STATUS_NO_UPDATE,
    OTA_STATUS_DOWNLOADING,
    OTA_STATUS_SUCCESS,
    OTA_STATUS_FAILED
};

struct ReleaseInfo {
    String tagName;
    String downloadUrl;
    String sha256Url;
    size_t size;
    bool found;
};

class GitHubUpdater {
public:
    static void init();
    static void loop();

    // Check both Core and Controller repositories and apply if available
    static bool checkForUpdates(bool force = false);

    // Specific repository check
    static bool checkAndApplyRepoUpdate(const String& org, const String& repo, 
                                        const String& currentVersion, const String& assetTag);

    // Manually trigger an update check immediately
    static void triggerCheck();

    // Query latest release info using memory-efficient stream parser
    static bool fetchLatestRelease(const String& org, const String& repo, 
                                   const String& assetTag, ReleaseInfo& outInfo);

    // Download and write binary to flash OTA partition
    static bool applyFirmwareUpdate(const String& downloadUrl, size_t binarySize);

    // Status getters
    static OtaUpdateStatus getStatus();
    static String getStatusString();
    static String getLastError();
    static uint32_t getLastCheckTime();

private:
    static OtaUpdateStatus _status;
    static String _lastError;
    static uint32_t _lastCheckMillis;
    static bool _forceCheckPending;

    // Follow redirect helper for GitHub 302 -> objects.githubusercontent.com
    static bool streamOtaFromUrl(const String& url, size_t expectedSize);
};
