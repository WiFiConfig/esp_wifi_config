#include <ESPWiFiConfig.h>
#include "esp_app_desc.h"

ESPWiFiConfig wifiConfig;

void setup() {
  Serial.begin(115200);
  auto config = ESPWiFiConfig::defaults();
  config.enable_ap = true;
  config.prov_ble.pop = "abcd1234"; // Choose a unique proof of possession for your product.
  esp_err_t result = wifiConfig.begin(config);
  if (result != ESP_OK) Serial.printf("Wi-Fi configuration failed: %s\n", esp_err_to_name(result));
}

void loop() {
  const auto *d = esp_app_get_description();
  Serial.printf("HEAP %u APP %02x%02x%02x%02x\n", ESP.getFreeHeap(), d->app_elf_sha256[0], d->app_elf_sha256[1], d->app_elf_sha256[2], d->app_elf_sha256[3]);
  delay(1000);
}
