#include <ESPWiFiConfig.h>
#include <WiFi.h>

ESPWiFiConfig wifiConfig;

void setup() {
  Serial.begin(115200);
  // Exercise adoption of interfaces initialized by Arduino first.
  WiFi.STA.begin(false);
  auto config = ESPWiFiConfig::defaults();
  config.provisioning_mode = WIFI_PROV_MANUAL;
  config.max_retry_per_network = 0; // Exercise lifecycle without joining saved networks.
  config.http_post_prov_mode = WIFI_HTTP_DISABLED;
  for (int cycle = 0; cycle < 10; ++cycle) {
    esp_err_t started = wifiConfig.begin(config);
    esp_err_t stopped = started == ESP_OK ? wifiConfig.end(cycle % 2 == 0) : started;
    Serial.printf("Cycle %d: begin=%s end=%s heap=%u\n", cycle,
                  esp_err_to_name(started), esp_err_to_name(stopped), ESP.getFreeHeap());
    delay(500);
  }
}

void loop() { delay(1000); }
