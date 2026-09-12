#include <Arduino.h>
#include <ESPWiFiConfig.h>

ESPWiFiConfig wifiConfig;
void setup() {
  Serial.begin(115200);
  esp_err_t result = wifiConfig.begin();
  if (result != ESP_OK) Serial.println(esp_err_to_name(result));
}
void loop() { delay(1000); }
