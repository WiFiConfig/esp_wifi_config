#pragma once
#ifndef ARDUINO_ARCH_ESP32
#error "Include esp_wifi_config.h in ESP-IDF applications"
#endif
#include <Arduino.h>
#include "esp_wifi_config.h"

/** A facade over the library's single Wi-Fi manager. No loop() polling needed.
 * Event callbacks run on the ESP event task; they must not block or call end().
 * Configuration strings, callback context, and a supplied Stream must outlive begin().
 */
class ESPWiFiConfig {
public:
    static wifi_cfg_config_t defaults() { return wifi_cfg_default_config(); }
    esp_err_t begin(const wifi_cfg_config_t &config);
    esp_err_t begin();
    esp_err_t end(bool stopWiFi = false);
    bool connected() const;
    wifi_status_t status() const;
    esp_err_t onEvent(esp_event_handler_t callback, void *context = nullptr);
    esp_err_t removeEvent(esp_event_handler_t callback);
    // Call before begin(); the application initializes the stream and its pins.
    // Only Improv may read from it while provisioning is running.
    esp_err_t setImprovSerial(Stream &stream);
};
