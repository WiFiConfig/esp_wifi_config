#pragma once
#include "esp_wifi_config_build.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifdef ARDUINO_ARCH_ESP32
esp_err_t wifi_cfg_platform_init(void);
esp_err_t wifi_cfg_platform_set_mode(wifi_mode_t mode);
void wifi_cfg_platform_release(bool stop_wifi);
int wifi_cfg_arduino_serial_read(uint8_t *byte);
void wifi_cfg_arduino_serial_write(const uint8_t *data, size_t len);
bool wifi_cfg_arduino_serial_ready(void);
#else
static inline esp_err_t wifi_cfg_platform_set_mode(wifi_mode_t mode)
{
    return esp_wifi_set_mode(mode);
}
#endif
#ifdef __cplusplus
}
#endif
