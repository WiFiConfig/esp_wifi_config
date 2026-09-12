// Same application for stock BLE and NimBLE-Arduino; only build flags differ.
// Driven by test_harness/rig/tools/arduino_ble_memory.py.
#include <ESPWiFiConfig.h>
#include <atomic>
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

ESPWiFiConfig manager;
static std::atomic<unsigned> advertising_ms{0};
static unsigned started_ms;
static bool running;
static void event(void *, esp_event_base_t, int32_t id, void *) {
  if (id == WIFI_CFG_EVENT_PROVISIONING_STARTED) advertising_ms = millis() - started_ms;
}
static void sample(const char *phase, esp_err_t result = ESP_OK) {
  auto *d = esp_app_get_description();
  Serial.printf("@MEM {\"phase\":\"%s\",\"app_sha\":\"%02x%02x%02x%02x\",\"nimble_arduino\":%d,\"improv\":%d,\"heap\":%u,\"minimum\":%u,\"largest\":%u,\"internal\":%u,\"advertising_ms\":%u,\"connected\":%s,\"result\":%d}\n",
    phase, d->app_elf_sha256[0], d->app_elf_sha256[1], d->app_elf_sha256[2], d->app_elf_sha256[3],
    WIFI_CFG_ARDUINO_NIMBLE, WIFI_CFG_ARDUINO_IMPROV_BLE,
    ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    advertising_ms.load(), running && manager.connected() ? "true" : "false", result);
}
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(500);
  esp_log_level_set("*", ESP_LOG_NONE);
}
void loop() {
  if (!Serial.available()) { delay(10); return; }
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command == "START") {
    sample("before");
    auto config = ESPWiFiConfig::defaults();
    config.enable_ap = true;
    config.provisioning_mode = WIFI_PROV_ON_FAILURE;
    config.prov_ble.pop = "abcd1234";
    config.prov_ble.disable_reboot_on_provisioning_success = true;
    config.prov_ble.disable_disconnect_restart = true;
    config.prov_ble.memory_policy = WIFI_CFG_PROV_MEM_KEEP_ALL;
    started_ms = millis();
    advertising_ms = 0;
    manager.onEvent(event);
    esp_err_t result = manager.begin(config);
    running = result == ESP_OK;
    sample("started", result);
  } else if (command == "END") {
    // Start the next cycle unprovisioned without rebooting the BLE host.
    esp_err_t reset = wifi_cfg_factory_reset();
    esp_err_t result = manager.end(true);
    if (reset != ESP_OK) result = reset;
    running = false;
    manager.removeEvent(event);
    delay(1000); // Let the idle task reclaim task stacks.
    sample("stopped", result);
  } else if (command.startsWith("SNAP ")) {
    sample(command.c_str() + 5);
  }
}
