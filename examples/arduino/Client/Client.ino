#include <ESPWiFiConfig.h>
#include <WiFi.h>
#include <HTTPClient.h>

ESPWiFiConfig wifiConfig;

void setup() {
  Serial.begin(115200);
  esp_err_t result = wifiConfig.begin();
  if (result != ESP_OK) Serial.println(esp_err_to_name(result));
}

void loop() {
  if (wifiConfig.connected() && WiFi.status() == WL_CONNECTED) {
    NetworkClient client;
    HTTPClient http;
    http.setTimeout(5000);
    if (http.begin(client, "http://example.com/")) {
      Serial.printf("HTTP result: %d\n", http.GET());
      http.end();
    }
  }
  delay(30000);
}
