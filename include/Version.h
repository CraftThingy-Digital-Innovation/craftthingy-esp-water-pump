#pragma once

#include "CraftThingyConfig.h"

#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 0

#define FIRMWARE_VERSION_STR "1.0.0"
#define FIRMWARE_BUILD_DATE __DATE__ " " __TIME__

inline const char* getFirmwareVersion() {
    return FIRMWARE_VERSION_STR;
}

inline const char* getBoardTargetTag() {
    return ASSET_TAG;
}
