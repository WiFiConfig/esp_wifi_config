// Driven by test_harness/rig/tools/arduino_interop.py on an isolated bench AP.
#include <ESPWiFiConfig.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <atomic>
#include "esp_app_desc.h"
#include "esp_log.h"

ESPWiFiConfig manager;
static std::atomic<unsigned> connectedEvents{0};

static void onEvent(void *, esp_event_base_t, int32_t id, void *) {
  if (id == WIFI_CFG_EVENT_CONNECTED) ++connectedEvents;
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1000);
  esp_log_level_set("*", ESP_LOG_NONE);
}

void loop() {
  if (!Serial.available()) { delay(10); return; }
  String command = Serial.readStringUntil('\n');
  char ssid[33], password[65], host[40];
  unsigned port;
  if (sscanf(command.c_str(), "RUN %32s %64s %39s %u", ssid, password, host, &port) != 4) return;
  const auto *desc = esp_app_get_description();
  Serial.printf("@ARDUINO {\"kind\":\"boot\",\"app_sha\":\"%02x%02x%02x%02x\"}\n",
                desc->app_elf_sha256[0], desc->app_elf_sha256[1],
                desc->app_elf_sha256[2], desc->app_elf_sha256[3]);
  manager.onEvent(onEvent);
  // First adopt interfaces created by Arduino; later cycles alternate between
  // retaining a connected station and shutting the radio down completely.
  WiFi.STA.begin(false);
  for (unsigned cycle = 0; cycle < 10; ++cycle) {
    bool priorReconnect = cycle % 2 == 0;
    bool stopRadio = cycle % 2 != 0;
    WiFi.setAutoReconnect(priorReconnect);
    auto cfg = ESPWiFiConfig::defaults();
    cfg.provisioning_mode = WIFI_PROV_MANUAL;
    cfg.http_post_prov_mode = WIFI_HTTP_DISABLED;
    cfg.max_retry_per_network = 0; // Suppress automatic attempts; connect(ssid) is explicit.
    unsigned beforeEvents = connectedEvents.load();
    esp_err_t start = manager.begin(cfg);
    wifi_network_t network = {};
    strlcpy(network.ssid, ssid, sizeof(network.ssid));
    strlcpy(network.password, password, sizeof(network.password));
    wifi_network_t saved = {};
    esp_err_t add = start;
    if (start == ESP_OK) {
      add = cycle == 0 ? wifi_cfg_add_network(&network) : wifi_cfg_get_network(ssid, &saved);
      if (cycle > 0 && add == ESP_OK && strcmp(saved.password, password) != 0) add = ESP_FAIL;
    }
    esp_err_t connect = add == ESP_OK ? wifi_cfg_connect(ssid) : add;
    esp_err_t wait = connect == ESP_OK ? wifi_cfg_wait_connected(30000) : connect;
    bool status = wait == ESP_OK && manager.connected() && WiFi.status() == WL_CONNECTED;
    bool ownsRetry = !WiFi.getAutoReconnect();
    int httpCode = -1;
    bool body = false;
    if (status) {
      NetworkClient client;
      HTTPClient http;
      http.setTimeout(5000);
      String url = String("http://") + host + ":" + port + "/arduino-interop";
      if (http.begin(client, url)) {
        httpCode = http.GET();
        body = http.getString() == "arduino-interop-ok";
        http.end();
      }
    }
    esp_err_t stop = start == ESP_OK ? manager.end(stopRadio) : start;
    delay(500); // Allow deferred event handling and task memory reclamation.
    bool restored = WiFi.getAutoReconnect() == priorReconnect;
    bool release = stopRadio ? WiFi.getMode() == WIFI_MODE_NULL : WiFi.status() == WL_CONNECTED;
    Serial.printf("@ARDUINO {\"kind\":\"cycle\",\"cycle\":%u,\"begin\":%d,\"wait\":%d,\"end\":%d,\"status\":%s,\"http\":%d,\"body\":%s,\"events\":%u,\"owns_retry\":%s,\"restored\":%s,\"release\":%s,\"heap\":%u}\n",
      cycle, start, wait, stop, status ? "true" : "false", httpCode, body ? "true" : "false",
      connectedEvents.load() - beforeEvents, ownsRetry ? "true" : "false",
      restored ? "true" : "false", release ? "true" : "false", ESP.getFreeHeap());
    if (start != ESP_OK || wait != ESP_OK || stop != ESP_OK) break;
  }
  manager.removeEvent(onEvent);
  Serial.println("@ARDUINO {\"kind\":\"done\"}");
}
