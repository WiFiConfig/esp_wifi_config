#pragma once

#include "esp_wifi_config_build.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if WIFI_CFG_ARDUINO_NIMBLE && WIFI_CFG_ARDUINO_PROV_BLE
#include "network_provisioning/manager.h"
#include "esp_event.h"
ESP_EVENT_DECLARE_BASE(WIFI_CFG_NIMBLE_PROV_EVENT);
extern const network_prov_scheme_t wifi_cfg_nimble_prov_scheme;
esp_err_t wifi_cfg_nimble_prov_set_uuid(uint8_t *uuid);
esp_err_t wifi_cfg_nimble_prov_set_mfg_data(uint8_t *data, ssize_t length);
#endif

#if WIFI_CFG_ARDUINO_NIMBLE && WIFI_CFG_ARDUINO_IMPROV_BLE
esp_err_t wifi_cfg_nimble_improv_enqueue(const uint8_t *data, size_t length);
void wifi_cfg_nimble_improv_result(const uint8_t *data, size_t length);
void wifi_cfg_nimble_improv_state(uint8_t state, uint8_t error);
uint16_t wifi_cfg_nimble_improv_mtu(void);
#endif

#ifdef __cplusplus
}
#endif
