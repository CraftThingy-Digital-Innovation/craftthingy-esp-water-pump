#include "CraftThingyCrypto.h"

#if defined(ESP8266)
    #include <bearssl/bearssl_hmac.h>
    #include <bearssl/bearssl_hash.h>
    extern "C" {
        #include <user_interface.h>
    }
#elif defined(ESP32)
    #include <mbedtls/md.h>
    #include <mbedtls/sha256.h>
    #include <esp_system.h>
#endif

uint32_t CraftThingyCrypto::_lastSequence = 0;
String CraftThingyCrypto::_recentNonces[CRAFT_MAX_NONCE_CACHE];
uint8_t CraftThingyCrypto::_nonceIndex = 0;

void CraftThingyCrypto::init() {
    for (size_t i = 0; i < CRAFT_MAX_NONCE_CACHE; ++i) {
        _recentNonces[i] = "";
    }
    _lastSequence = 0;
}

void CraftThingyCrypto::bytesToHex(const uint8_t* in, size_t len, char* out) {
    static const char hexChars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = hexChars[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hexChars[in[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

bool CraftThingyCrypto::hexToBytes(const char* hex, uint8_t* out, size_t maxLen) {
    size_t hexLen = strlen(hex);
    if (hexLen % 2 != 0 || hexLen / 2 > maxLen) return false;

    auto hexNibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    for (size_t i = 0; i < hexLen; i += 2) {
        int hi = hexNibble(hex[i]);
        int lo = hexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool CraftThingyCrypto::constantTimeEquals(const String& a, const String& b) {
    if (a.length() != b.length()) return false;
    unsigned char result = 0;
    for (size_t i = 0; i < a.length(); ++i) {
        result |= (a[i] ^ b[i]);
    }
    return result == 0;
}

bool CraftThingyCrypto::sha256(const uint8_t* data, size_t len, uint8_t outDigest[32]) {
#if defined(ESP8266)
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, data, len);
    br_sha256_out(&ctx, outDigest);
    return true;
#elif defined(ESP32)
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
        mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256
        mbedtls_sha256_update(&ctx, data, len);
        mbedtls_sha256_finish(&ctx, outDigest);
    #else
        mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, data, len);
        mbedtls_sha256_finish_ret(&ctx, outDigest);
    #endif
    mbedtls_sha256_free(&ctx);
    return true;
#else
    return false;
#endif
}

bool CraftThingyCrypto::sha256Hex(const String& data, String& outHex) {
    uint8_t digest[32];
    if (!sha256((const uint8_t*)data.c_str(), data.length(), digest)) return false;
    char hex[65];
    bytesToHex(digest, 32, hex);
    outHex = String(hex);
    return true;
}

bool CraftThingyCrypto::hmacSha256(const uint8_t* key, size_t keyLen, 
                                   const uint8_t* msg, size_t msgLen, 
                                   uint8_t outDigest[32]) {
#if defined(ESP8266)
    br_hmac_key_context kc;
    br_hmac_context ctx;
    br_hmac_key_init(&kc, &br_sha256_vtable, key, keyLen);
    br_hmac_init(&ctx, &kc, 0);
    br_hmac_update(&ctx, msg, msgLen);
    br_hmac_out(&ctx, outDigest);
    return true;
#elif defined(ESP32)
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1) != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, msg, msgLen);
    mbedtls_md_hmac_finish(&ctx, outDigest);
    mbedtls_md_free(&ctx);
    return true;
#else
    return false;
#endif
}

bool CraftThingyCrypto::hmacSha256Hex(const String& key, const String& msg, String& outHex) {
    uint8_t digest[32];
    if (!hmacSha256((const uint8_t*)key.c_str(), key.length(), 
                    (const uint8_t*)msg.c_str(), msg.length(), digest)) {
        return false;
    }
    char hex[65];
    bytesToHex(digest, 32, hex);
    outHex = String(hex);
    return true;
}

bool CraftThingyCrypto::deriveNextRollingKey(const String& currentKey, uint32_t counter, String& nextKey) {
    String message = "CRAFT_ROLL_KEY_" + String(counter);
    return hmacSha256Hex(currentKey, message, nextKey);
}

String CraftThingyCrypto::generateRandomHex(size_t byteCount) {
    String hex = "";
    static const char hexChars[] = "0123456789abcdef";
    for (size_t i = 0; i < byteCount; ++i) {
        uint32_t r = 0;
#if defined(ESP8266)
        r = os_random();
#elif defined(ESP32)
        r = esp_random();
#endif
        uint8_t b = (uint8_t)(r & 0xFF);
        hex += hexChars[(b >> 4) & 0x0F];
        hex += hexChars[b & 0x0F];
    }
    return hex;
}

bool CraftThingyCrypto::checkAndRecordNonce(const String& nonce, uint32_t sequence) {
    if (nonce.length() == 0) return false;

    // Check if nonce was recently seen
    for (size_t i = 0; i < CRAFT_MAX_NONCE_CACHE; ++i) {
        if (_recentNonces[i].length() > 0 && _recentNonces[i] == nonce) {
            return false; // Replay detected!
        }
    }

    // Sequence check (if non-zero sequence provided, must be strictly increasing)
    if (sequence > 0) {
        if (sequence <= _lastSequence) {
            return false; // Stale sequence counter!
        }
        _lastSequence = sequence;
    }

    // Record nonce in ring buffer
    _recentNonces[_nonceIndex] = nonce;
    _nonceIndex = (_nonceIndex + 1) % CRAFT_MAX_NONCE_CACHE;
    return true;
}

bool CraftThingyCrypto::verifyRequestSignature(const String& key,
                                               const String& method,
                                               const String& path,
                                               const String& timestampStr,
                                               const String& nonce,
                                               const String& body,
                                               const String& signatureHex) {
    if (key.length() == 0 || signatureHex.length() != 64) return false;

    // Payload to sign: METHOD\nPATH\nTIMESTAMP\nNONCE\nBODY
    String payload = method + "\n" + path + "\n" + timestampStr + "\n" + nonce + "\n" + body;

    String expectedSignature;
    if (!hmacSha256Hex(key, payload, expectedSignature)) {
        return false;
    }

    // Constant-time compare
    return constantTimeEquals(expectedSignature, signatureHex);
}
