#include <ESPWiFiConfig.h>

ESPWiFiConfig wifiConfig;
wifi_network_t networks[] = {{"MyWiFi", "password", 10}};

void setup() {
  Serial.begin(115200);
  auto config = ESPWiFiConfig::defaults();
  config.default_networks = networks;
  config.default_network_count = 1;
  config.http_post_prov_mode = WIFI_HTTP_DISABLED;
  esp_err_t result = wifiConfig.begin(config);
  if (result != ESP_OK) Serial.println(esp_err_to_name(result));
}

void loop() { delay(1000); }
