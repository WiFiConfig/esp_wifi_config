#include "esp_wifi_config_build.h"
#ifdef ARDUINO_ARCH_ESP32
#include "ESPWiFiConfig.h"
#include "esp_wifi_config_platform.h"
#include <WiFi.h>

#if WIFI_CFG_ARDUINO_PROV_BLE || WIFI_CFG_ARDUINO_IMPROV_BLE
// Reserve BLE memory before initArduino() runs, even though our BLE code is C.
#include "esp32-hal-alloc-ble-mem.h"
#endif

static bool platform_active;
static bool previous_reconnect;
static Stream *improv_stream;

extern "C" esp_err_t wifi_cfg_platform_init(void)
{
    previous_reconnect = WiFi.getAutoReconnect();
    WiFi.setAutoReconnect(false);
    // Arduino must initialize its own netifs and status/event bookkeeping first.
    // begin(false) starts STA without reconnecting to Arduino's saved network.
    if (!WiFi.STA.begin(false)) {
        WiFi.setAutoReconnect(previous_reconnect);
        return ESP_FAIL;
    }
    // A pre-existing association belongs to the previous connection manager.
    // Finish its disconnect before our C task selects a saved network. Do not
    // erase the SDK credentials or destroy Arduino's interfaces.
    if (!WiFi.STA.disconnect(false, 1000)) {
        WiFi.setAutoReconnect(previous_reconnect);
        return ESP_ERR_TIMEOUT;
    }
    platform_active = true;
    return ESP_OK;
}

extern "C" esp_err_t wifi_cfg_platform_set_mode(wifi_mode_t mode)
{
    return WiFi.mode(mode) ? ESP_OK : ESP_FAIL;
}

extern "C" void wifi_cfg_platform_release(bool stop_wifi)
{
    if (!platform_active) return;
    if (stop_wifi) WiFi.mode(WIFI_MODE_NULL);
    WiFi.setAutoReconnect(previous_reconnect);
    platform_active = false;
}

extern "C" bool wifi_cfg_arduino_serial_ready(void) { return improv_stream != nullptr; }
extern "C" int wifi_cfg_arduino_serial_read(uint8_t *byte)
{
    if (!improv_stream) return 0;
    int value = improv_stream->read();
    if (value < 0) { delay(1); return 0; }
    *byte = static_cast<uint8_t>(value);
    return 1;
}
extern "C" void wifi_cfg_arduino_serial_write(const uint8_t *data, size_t len)
{
    if (improv_stream) improv_stream->write(data, len);
}

esp_err_t ESPWiFiConfig::begin(const wifi_cfg_config_t &config) { return wifi_cfg_init(&config); }
esp_err_t ESPWiFiConfig::begin()
{
    auto config = defaults();
    config.enable_ap = WIFI_CFG_ARDUINO_SOFTAP;
    return begin(config);
}
esp_err_t ESPWiFiConfig::end(bool stopWiFi) { return wifi_cfg_deinit(stopWiFi); }
wifi_status_t ESPWiFiConfig::status() const
{
    wifi_status_t value = {};
    wifi_cfg_get_status(&value);
    return value;
}
bool ESPWiFiConfig::connected() const { return status().state == WIFI_STATE_CONNECTED; }
esp_err_t ESPWiFiConfig::onEvent(esp_event_handler_t callback, void *context)
{
    if (!callback) return ESP_ERR_INVALID_ARG;
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    return esp_event_handler_register(WIFI_CFG_EVENT, ESP_EVENT_ANY_ID, callback, context);
}
esp_err_t ESPWiFiConfig::removeEvent(esp_event_handler_t callback)
{
    return esp_event_handler_unregister(WIFI_CFG_EVENT, ESP_EVENT_ANY_ID, callback);
}
esp_err_t ESPWiFiConfig::setImprovSerial(Stream &stream)
{
#if WIFI_CFG_ARDUINO_IMPROV_SERIAL
    if (platform_active) return ESP_ERR_INVALID_STATE;
    improv_stream = &stream;
    return ESP_OK;
#else
    (void)stream;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
#endif
