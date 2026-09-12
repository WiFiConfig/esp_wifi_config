#include <ESPWiFiConfig.h>

ESPWiFiConfig wifiConfig;

void setup() {
  Serial.begin(115200);
  auto config = ESPWiFiConfig::defaults();
  config.enable_ap = true;
  esp_err_t result = wifiConfig.begin(config);
  if (result != ESP_OK) Serial.printf("Wi-Fi configuration failed: %s\n", esp_err_to_name(result));
}

void loop() { delay(1000); }
