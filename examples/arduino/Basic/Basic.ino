#include <ESPWiFiConfig.h>
#include <WiFi.h>

ESPWiFiConfig wifiConfig;

void setup() {
  Serial.begin(115200);
  // With no saved network, browse to 192.168.4.1 on the ESP32-Config AP.
  esp_err_t result = wifiConfig.begin();
  if (result != ESP_OK) Serial.printf("Wi-Fi configuration failed: %s\n", esp_err_to_name(result));
}

void loop() {
  if (wifiConfig.connected()) {
    Serial.printf("Connected to %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }
  delay(5000);
}
