#include <ESPWiFiConfig.h>

ESPWiFiConfig wifiConfig;

void onWiFiEvent(void *, esp_event_base_t, int32_t id, void *) {
  // Called from the event task: do not block or call end() here.
  if (id == WIFI_CFG_EVENT_VAR_CHANGED) Serial.println("A saved variable changed");
}

void setup() {
  Serial.begin(115200);
  wifiConfig.onEvent(onWiFiEvent);
  if (wifiConfig.begin() != ESP_OK) return;
  char value[128];
  if (wifi_cfg_get_var("device_name", value, sizeof(value)) != ESP_OK) {
    wifi_cfg_set_var("device_name", "My ESP32");
  }
  // Variables are also editable through the existing REST/BLE APIs.
}

void loop() { delay(1000); }
