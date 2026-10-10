#pragma once

#include <Arduino.h>
#include "CraftThingyConfig.h"

class CraftThingyCrypto {
public:
    static void init();

    // Compute HMAC-SHA256 over a message using a key
    static bool hmacSha256(const uint8_t* key, size_t keyLen, 
                           const uint8_t* msg, size_t msgLen, 
                           uint8_t outDigest[32]);

    static bool hmacSha256Hex(const String& key, const String& msg, String& outHex);

    // Constant-time comparison to prevent timing attacks
    static bool constantTimeEquals(const String& a, const String& b);

    // Compute SHA-256 hash of a payload
    static bool sha256(const uint8_t* data, size_t len, uint8_t outDigest[32]);
    static bool sha256Hex(const String& data, String& outHex);

    // Rolling Key Ratchet: derive next key from current key and sequence counter
    static bool deriveNextRollingKey(const String& currentKey, uint32_t counter, String& nextKey);

    // Generate cryptographically secure random hex string using hardware RNG
    static String generateRandomHex(size_t byteCount);

    // Verify incoming HTTP API Request signature
    // Payload signed: method + "\n" + path + "\n" + timestamp + "\n" + nonce + "\n" + body
    static bool verifyRequestSignature(const String& key,
                                       const String& method,
                                       const String& path,
                                       const String& timestampStr,
                                       const String& nonce,
                                       const String& body,
                                       const String& signatureHex);

    // Anti-replay filter check: rejects already-seen nonces or invalid sequence counters
    static bool checkAndRecordNonce(const String& nonce, uint32_t sequence);

    // Hex conversion helpers
    static void bytesToHex(const uint8_t* in, size_t len, char* out);
    static bool hexToBytes(const char* hex, uint8_t* out, size_t maxLen);

private:
    static uint32_t _lastSequence;
    static String _recentNonces[CRAFT_MAX_NONCE_CACHE];
    static uint8_t _nonceIndex;
};
