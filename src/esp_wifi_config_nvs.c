/**
 * @file esp_wifi_config_nvs.c
 * @brief NVS storage for WiFi Config
 */

#include "esp_wifi_config_priv.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "wifi_cfg_nvs";

// =============================================================================
// NVS Keys
// =============================================================================

#define NVS_KEY_NET_COUNT       "net_count"
#define NVS_KEY_NET_PREFIX      "net_"
#define NVS_KEY_VAR_COUNT       "var_count"
#define NVS_KEY_VAR_PREFIX      "var_"
#define NVS_KEY_AP_CONFIG       "ap_config"
#define NVS_KEY_AUTH_USER       "auth_user"
#define NVS_KEY_AUTH_PASS       "auth_pass"

// =============================================================================
// Init
// =============================================================================

esp_err_t wifi_cfg_nvs_init(void)
{
    // NVS may have already been initialized by another component
    esp_err_t ret = nvs_flash_init();
#ifndef ARDUINO_ARCH_ESP32
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
#endif // Arduino owns initialization/recovery of the shared NVS partition.
    // ESP_ERR_NVS_INVALID_STATE = already initialized, OK
    if (ret == ESP_OK || ret == ESP_ERR_NVS_INVALID_STATE) {
        return ESP_OK;
    }
    return ret;
}

// =============================================================================
// Blob arrays (networks, vars)
// =============================================================================
//
// Both live on flash in the same shape: a u8 element count under `count_key`,
// then one blob per element keyed `prefix` + decimal index. Only the key
// strings and the element size differ. The keys these build are byte-identical
// to the ones the four entry points below used to format inline -- the format
// is still "<prefix>%d" over the same 16-byte buffer -- so data written by
// earlier firmware reads back unchanged.

static esp_err_t nvs_load_blobs(const char *count_key, const char *prefix,
                                void *items, size_t item_size,
                                size_t max_count, size_t *count,
                                const char *what)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(WIFI_CFG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *count = 0;
        return ESP_OK;
    }
    if (ret != ESP_OK) return ret;

    uint8_t stored_count = 0;
    ret = nvs_get_u8(handle, count_key, &stored_count);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        stored_count = 0;
        ret = ESP_OK;
    }

    size_t loaded = 0;
    for (uint8_t i = 0; i < stored_count && loaded < max_count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", prefix, i);

        size_t len = item_size;
        ret = nvs_get_blob(handle, key, (char *)items + loaded * item_size, &len);
        if (ret == ESP_OK) {
            loaded++;
        }
    }

    *count = loaded;
    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded %zu %s from NVS", loaded, what);
    return ESP_OK;
}

static esp_err_t nvs_save_blobs(const char *count_key, const char *prefix,
                                const void *items, size_t item_size,
                                size_t count, const char *what)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(WIFI_CFG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    // Save count
    ret = nvs_set_u8(handle, count_key, (uint8_t)count);
    if (ret != ESP_OK) goto cleanup;

    // Save each element
    for (size_t i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", prefix, (int)i);
        ret = nvs_set_blob(handle, key, (const char *)items + i * item_size, item_size);
        if (ret != ESP_OK) goto cleanup;
    }

    ret = nvs_commit(handle);
    ESP_LOGI(TAG, "Saved %zu %s to NVS", count, what);

cleanup:
    nvs_close(handle);
    return ret;
}

// =============================================================================
// Networks
// =============================================================================

esp_err_t wifi_cfg_nvs_load_networks(wifi_network_t *networks, size_t max_count, size_t *count)
{
    return nvs_load_blobs(NVS_KEY_NET_COUNT, NVS_KEY_NET_PREFIX, networks,
                          sizeof(wifi_network_t), max_count, count, "networks");
}

esp_err_t wifi_cfg_nvs_save_networks(const wifi_network_t *networks, size_t count)
{
    return nvs_save_blobs(NVS_KEY_NET_COUNT, NVS_KEY_NET_PREFIX, networks,
                          sizeof(wifi_network_t), count, "networks");
}

// =============================================================================
// Variables
// =============================================================================

esp_err_t wifi_cfg_nvs_load_vars(wifi_var_t *vars, size_t max_count, size_t *count)
{
    return nvs_load_blobs(NVS_KEY_VAR_COUNT, NVS_KEY_VAR_PREFIX, vars,
                          sizeof(wifi_var_t), max_count, count, "vars");
}

esp_err_t wifi_cfg_nvs_save_vars(const wifi_var_t *vars, size_t count)
{
    return nvs_save_blobs(NVS_KEY_VAR_COUNT, NVS_KEY_VAR_PREFIX, vars,
                          sizeof(wifi_var_t), count, "vars");
}

// =============================================================================
// AP Config
// =============================================================================

esp_err_t wifi_cfg_nvs_load_ap_config(wifi_cfg_ap_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(WIFI_CFG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) return ret;
    
    size_t len = sizeof(wifi_cfg_ap_config_t);
    ret = nvs_get_blob(handle, NVS_KEY_AP_CONFIG, config, &len);
    
    nvs_close(handle);
    return ret;
}

esp_err_t wifi_cfg_nvs_save_ap_config(const wifi_cfg_ap_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(WIFI_CFG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    
    ret = nvs_set_blob(handle, NVS_KEY_AP_CONFIG, config, sizeof(wifi_cfg_ap_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    
    nvs_close(handle);
    ESP_LOGI(TAG, "Saved AP config to NVS");
    return ret;
}

// =============================================================================
// Auth Credentials
// =============================================================================

esp_err_t wifi_cfg_nvs_load_auth(char *username, size_t ulen, char *password, size_t plen)
{
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(WIFI_CFG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) return ret;
    
    size_t len = ulen;
    ret = nvs_get_str(handle, NVS_KEY_AUTH_USER, username, &len);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ret;
    }
    
    len = plen;
    ret = nvs_get_str(handle, NVS_KEY_AUTH_PASS, password, &len);
    
    nvs_close(handle);
    return ret;
}

esp_err_t wifi_cfg_nvs_save_auth(const char *username, const char *password)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(WIFI_CFG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    if (username) {
        ret = nvs_set_str(handle, NVS_KEY_AUTH_USER, username);
        if (ret != ESP_OK) goto cleanup;
    }

    if (password) {
        ret = nvs_set_str(handle, NVS_KEY_AUTH_PASS, password);
        if (ret != ESP_OK) goto cleanup;
    }

    ret = nvs_commit(handle);

cleanup:
    nvs_close(handle);
    return ret;
}

// =============================================================================
// Factory Reset
// =============================================================================

esp_err_t wifi_cfg_nvs_factory_reset(void)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(WIFI_CFG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_erase_all(handle);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Factory reset: NVS namespace erased");
    return ret;
}

