#include <Arduino.h>
#include <CraftThingyCore.h>
#include "WaterPumpController.h"

// Hardware Pin Assignment:
// - ESP8266 (ESP-01 / ESP-01S):
//   Automatically self-detected at runtime via capacitive pin fingerprinting:
//   * Legacy Schema: Relay GPIO 0, Float Sensor GPIO 3 (RX)
//   * PZEM Schema (1µF / 10µF Boot Circuit): Relay GPIO 0, Float Sensor GPIO 2, PZEM UART GPIO 1 (TX) & 3 (RX)
// - ESP32:
//   GPIO 23 -> Relay, GPIO 22 -> Float Sensor
#if defined(ESP8266)
    static WaterPumpController pumpController(0, 2, true);
#else
    static WaterPumpController pumpController(23, 22, true);
#endif

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F(" CraftThingy - Automated Water Pump Controller   "));
    Serial.println(F(" Auto-Detect: Legacy (G3), PZEM-1uF, PZEM-10uF   "));
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
