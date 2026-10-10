#pragma once

#include <Arduino.h>
#include "CraftThingyConfig.h"

#if defined(ESP8266)
    #include <ESP8266WebServer.h>
    typedef ESP8266WebServer ApiServerType;
#elif defined(ESP32)
    #include <WebServer.h>
    typedef WebServer ApiServerType;
#endif

class RestApi {
public:
    static void init();
    static void loop();

    // Verify HMAC-SHA256 rolling key signature or Bearer/Basic Auth
    static bool isRequestAuthorized(ApiServerType& server, String& outMethod);

private:
    static ApiServerType _server;
    static void setupRoutes();

    // Handlers
    static void handleStatus();
    static void handleWifiScan();
    static void handleWifiConfig();
    static void handleGitHubConfig();
    static void handleTriggerUpdate();
    static void handleRotateKey();
    static void handleReboot();
    static void handleControllerStatus();
    static void handleControllerCommand();
    static void handleNotFound();

    // Helper JSON response senders
    static void sendJson(int code, const String& json);
    static void sendError(int code, const String& error);
    static void sendSuccess(const String& msg);
};
