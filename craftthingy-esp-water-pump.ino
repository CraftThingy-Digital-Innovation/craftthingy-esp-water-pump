#include <Arduino.h>
#include <CraftThingyCore.h>
#include "WaterPumpController.h"

// Hardware Pin Assignment:
// - ESP8266 (ESP-01 / ESP-01S):
//   GPIO 0 -> Relay (Active LOW on standard ESP-01 relay shields)
//   GPIO 3 (RX pin) or GPIO 2 -> Float Switch / Water Level Sensor
// - ESP32:
//   GPIO 23 -> Relay
//   GPIO 22 -> Float Switch / Water Level Sensor
#if defined(ESP8266)
    static WaterPumpController pumpController(0, 3, true);
#else
    static WaterPumpController pumpController(23, 22, true);
#endif

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F(" CraftThingy - Automated Water Pump Controller   "));
    Serial.println(F("=================================================="));

    // Configure role & target repos
    ConfigStore::init();
    ConfigStore::setActiveRole("water_pump");
    ConfigStore::setGitHubConfig("CraftThingy-Digital-Innovation", 
                                 "craftthingy-esp-core-ota", 
                                 "craftthingy-esp-water-pump", "");

    // Launch CraftThingyCore with water pump controller attached
    CraftThingyCore.begin(&pumpController);
}

void loop() {
    CraftThingyCore.loop();
}
