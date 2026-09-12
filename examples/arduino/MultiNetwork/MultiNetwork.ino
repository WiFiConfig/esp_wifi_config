#include <ESPWiFiConfig.h>

ESPWiFiConfig wifiConfig;
wifi_network_t networks[] = {{"PrimaryWiFi", "password", 10}, {"BackupWiFi", "password", 5}};

void setup() {
  Serial.begin(115200);
  auto config = ESPWiFiConfig::defaults();
  config.default_networks = networks;
  config.default_network_count = sizeof(networks) / sizeof(networks[0]);
  config.enable_ap = true;
  // Defaults seed an empty store. Later changes go through the portal or C API.
  esp_err_t result = wifiConfig.begin(config);
  if (result != ESP_OK) Serial.println(esp_err_to_name(result));
}

void loop() { delay(1000); }
