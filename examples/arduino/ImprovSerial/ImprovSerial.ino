#include <ESPWiFiConfig.h>
#include "esp_log.h"

ESPWiFiConfig wifiConfig;

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_NONE); // UART0 is reserved for binary Improv frames.
  auto config = ESPWiFiConfig::defaults();
  config.enable_ap = true;
  wifiConfig.setImprovSerial(Serial);
  // Reserve this stream for Improv; send application logs to another port.
  esp_err_t result = wifiConfig.begin(config);
  (void)result; // Report errors using an LED or a separate logging port.
}

void loop() { delay(1000); }
