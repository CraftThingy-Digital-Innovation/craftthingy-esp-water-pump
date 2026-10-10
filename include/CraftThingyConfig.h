#pragma once

#include <Arduino.h>

// =================================================================
// Architecture & Board Identification
// =================================================================
#if defined(ESP8266)
    #define CRAFT_ARCH_ESP8266 1
    #define DEFAULT_BOARD_TAG "esp8266"
    #define CRAFT_FLASH_SIZE_DESC "1MB Flash"
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
    #define CRAFT_ARCH_ESP32 1
    #define CRAFT_ARCH_ESP32S3 1
    #define DEFAULT_BOARD_TAG "esp32s3"
    #define CRAFT_FLASH_SIZE_DESC "ESP32-S3"
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
    #define CRAFT_ARCH_ESP32 1
    #define CRAFT_ARCH_ESP32C3 1
    #define DEFAULT_BOARD_TAG "esp32c3"
    #define CRAFT_FLASH_SIZE_DESC "ESP32-C3"
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ARDUINO_ESP32S2_DEV)
    #define CRAFT_ARCH_ESP32 1
    #define CRAFT_ARCH_ESP32S2 1
    #define DEFAULT_BOARD_TAG "esp32s2"
    #define CRAFT_FLASH_SIZE_DESC "ESP32-S2"
#elif defined(ESP32)
    #define CRAFT_ARCH_ESP32 1
    #define DEFAULT_BOARD_TAG "esp32"
    #define CRAFT_FLASH_SIZE_DESC "ESP32 Standard"
#else
    #error "Unsupported hardware architecture! Only ESP8266 and ESP32 variants are supported."
#endif

#ifndef ASSET_TAG
    #define ASSET_TAG DEFAULT_BOARD_TAG
#endif

// =================================================================
// Default Configuration Constants
// =================================================================
#define CRAFT_CORE_VERSION          "1.0.0"
#define CRAFT_DEFAULT_ORG           "CraftThingy-Digital-Innovation"
#define CRAFT_DEFAULT_CORE_REPO     "craftthingy-esp-core-ota"
#define CRAFT_DEFAULT_CONTROLLER_REPO "water-pump-controller"
#define CRAFT_DEFAULT_ROLE          "water_pump"

// Networking & Failover Defaults
#define CRAFT_WIFI_MAX_RETRIES      5
#define CRAFT_WIFI_RETRY_DELAY_MS   1500
#define CRAFT_AP_CHANNEL            1
#define CRAFT_DNS_PORT              53
#define CRAFT_WEB_PORT              80
#define CRAFT_OTA_CHECK_INTERVAL_S  3600    // Check every 1 hour by default

// Security & Rolling Key Parameters
#define CRAFT_SECRET_KEY_LEN        32
#define CRAFT_HASH_HEX_LEN          64
#define CRAFT_TIMESTAMP_WINDOW_S    300     // 5-minute replay window
#define CRAFT_MAX_NONCE_CACHE       16

// Buffer tuning for ESP8266 1MB Flash / RAM constraints
#if defined(ESP8266)
    #define CRAFT_TLS_BUFFER_SIZE   1024    // Reduced BearSSL buffer (saves ~28KB RAM!)
    #define CRAFT_STREAM_CHUNK_SIZE 512
#else
    #define CRAFT_TLS_BUFFER_SIZE   4096
    #define CRAFT_STREAM_CHUNK_SIZE 2048
#endif
