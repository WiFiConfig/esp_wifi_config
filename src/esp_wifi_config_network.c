/**
 * @file esp_wifi_config_network.c
 * @brief Network management, connection logic, scan
 */

#include "esp_wifi_config_priv.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "wifi_cfg_net";

#if WIFI_CFG_C5_BLE_SCAN_WORKAROUND
#warning "esp_wifi_config: ESP32-C5 BLE + 5 GHz scans can time out on ESP-IDF <5.5.0 (IDFGH-15407); BLE-time scans will be limited to 2.4 GHz. Upgrade to ESP-IDF 5.5.0+ for dual-band BLE provisioning scans."
#endif

#if CONFIG_IDF_TARGET_ESP32C5
// An all-band C5 scan can spend more than ten seconds on passive 5 GHz/DFS
// channels. Keep the usual bound on the single-band targets while allowing
// the C5 scan-done event enough time to arrive.
#define WIFI_CFG_SCAN_TIMEOUT_MS 25000
#else
#define WIFI_CFG_SCAN_TIMEOUT_MS 10000
#endif

// =============================================================================
// Connection Logic (runs in task context)
// =============================================================================

static void sort_networks_by_priority(void)
{
    // Avoid underflow when network_count < 2
    if (g_wifi_cfg->network_count < 2) return;
    
    // Simple bubble sort by priority (descending)
    for (size_t i = 0; i < g_wifi_cfg->network_count - 1; i++) {
        for (size_t j = 0; j < g_wifi_cfg->network_count - i - 1; j++) {
            if (g_wifi_cfg->networks[j].priority < g_wifi_cfg->networks[j + 1].priority) {
                wifi_network_t tmp = g_wifi_cfg->networks[j];
                g_wifi_cfg->networks[j] = g_wifi_cfg->networks[j + 1];
                g_wifi_cfg->networks[j + 1] = tmp;
            }
        }
    }
}

/**
 * @brief Build the STA config for one saved network
 *
 * `sta.threshold.authmode` left at zero lets ESP-IDF demand WPA2 for any
 * password of 8+ characters, which silently filters out WEP and WPA(v1)
 * routers. `sta_min_auth_mode` is the application's opt-in to lower (or
 * raise) that floor; an open network keeps the OPEN threshold regardless.
 */
static void fill_sta_config(wifi_config_t *wifi_cfg, const wifi_network_t *net)
{
    memset(wifi_cfg, 0, sizeof(*wifi_cfg));
    strncpy((char *)wifi_cfg->sta.ssid, net->ssid, sizeof(wifi_cfg->sta.ssid) - 1);
    strncpy((char *)wifi_cfg->sta.password, net->password, sizeof(wifi_cfg->sta.password) - 1);
    if (net->password[0] != '\0') {
        wifi_cfg->sta.threshold.authmode = g_wifi_cfg->config.sta_min_auth_mode;
    }
}

/**
 * @brief Calculate exponential backoff delay
 * @param retry Current retry number (0-based)
 * @return Delay in milliseconds
 */
uint32_t wifi_cfg_calc_backoff_delay(int retry)
{
    uint32_t base = g_wifi_cfg->config.retry_interval_ms;
    uint32_t max_delay = g_wifi_cfg->config.retry_max_interval_ms;

    // Exponential backoff: base * 2^retry
    uint32_t delay = base << retry;  // base * 2^retry

    // Cap at max delay
    if (delay > max_delay || delay < base) {  // overflow check
        delay = max_delay;
    }

    return delay;
}

void wifi_cfg_start_connect_sequence(void)
{
    if (wifi_cfg_stopping()) return;
    // A connect sequence is starting now, so any auto-reconnect scheduled for
    // later is redundant. Cancelling centrally (rather than at each of the
    // half-dozen call sites) keeps the manager task from re-entering this
    // function the moment it returns - it runs synchronously and can occupy
    // the task for tens of seconds, so a deadline set before the call would
    // otherwise be due by the time we get back to the loop.
    if (g_wifi_cfg) g_wifi_cfg->reconnect_pending = false;

    if (!g_wifi_cfg || g_wifi_cfg->network_count == 0) {
        if (g_wifi_cfg &&
            (g_wifi_cfg->config.provisioning_mode == WIFI_PROV_ON_FAILURE ||
             g_wifi_cfg->config.provisioning_mode == WIFI_PROV_WHEN_UNPROVISIONED)) {
            if (!g_wifi_cfg->provisioning_active) {
                ESP_LOGI(TAG, "No networks, starting provisioning");
                wifi_cfg_prov_orchestrate_start();
            }
        }
        return;
    }

    wifi_cfg_lock();
    g_wifi_cfg->connecting = true;
    sort_networks_by_priority();
    wifi_cfg_unlock();

    // Try each network (AP can run in parallel - AP+STA mode)
    for (size_t i = 0; i < g_wifi_cfg->network_count; i++) {
        wifi_network_t *net = &g_wifi_cfg->networks[i];
        ESP_LOGI(TAG, "Trying network: %s (priority: %d)", net->ssid, net->priority);

        g_wifi_cfg->state = WIFI_STATE_CONNECTING;
        g_wifi_cfg->current_network_idx = i;
        wifi_cfg_event_post(WIFI_CFG_EVENT_CONNECTING, net->ssid, strlen(net->ssid) + 1);

        wifi_config_t wifi_cfg;
        fill_sta_config(&wifi_cfg, net);

        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

        for (int retry = 0; retry < g_wifi_cfg->config.max_retry_per_network; retry++) {
            if (wifi_cfg_stopping()) return;
            xEventGroupClearBits(g_wifi_cfg->event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
                continue;
            }

            // Wait for connection result
            EventBits_t bits = xEventGroupWaitBits(g_wifi_cfg->event_group,
                                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_STOPPING_BIT,
                                                   pdFALSE, pdFALSE,
                                                   pdMS_TO_TICKS(15000));

            if (bits & WIFI_STOPPING_BIT) return;

            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "Connected to %s", net->ssid);
                g_wifi_cfg->connecting = false;
                g_wifi_cfg->retry_count = 0;  // Reset backoff counter on success
                return;
            }

            ESP_LOGW(TAG, "Failed to connect to %s, retry %d/%d",
                     net->ssid, retry + 1, g_wifi_cfg->config.max_retry_per_network);

            if (retry < g_wifi_cfg->config.max_retry_per_network - 1) {
                uint32_t delay = wifi_cfg_calc_backoff_delay(retry);
                ESP_LOGI(TAG, "Backoff delay: %lu ms", (unsigned long)delay);
                if (xEventGroupWaitBits(g_wifi_cfg->event_group, WIFI_STOPPING_BIT,
                                        pdFALSE, pdFALSE, pdMS_TO_TICKS(delay)) & WIFI_STOPPING_BIT) return;
            }
        }
    }

    g_wifi_cfg->connecting = false;

    ESP_LOGW(TAG, "Failed to connect to any network");
    g_wifi_cfg->state = WIFI_STATE_DISCONNECTED;

    // Start provisioning on failure if mode is ON_FAILURE and not already provisioning
    if (g_wifi_cfg->config.provisioning_mode == WIFI_PROV_ON_FAILURE &&
        !g_wifi_cfg->provisioning_active) {
        ESP_LOGI(TAG, "All networks failed, starting provisioning");
        wifi_cfg_prov_orchestrate_start();
    }
}

// =============================================================================
// Network Management
// =============================================================================

esp_err_t wifi_cfg_add_network(const wifi_network_t *network)
{
    if (!g_wifi_cfg || !network) return ESP_ERR_INVALID_ARG;
    
    wifi_cfg_lock();
    
    // Check if exists
    for (size_t i = 0; i < g_wifi_cfg->network_count; i++) {
        if (strcmp(g_wifi_cfg->networks[i].ssid, network->ssid) == 0) {
            wifi_cfg_unlock();
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    // Check capacity
    if (g_wifi_cfg->network_count >= WIFI_CFG_MAX_NETWORKS) {
        wifi_cfg_unlock();
        return ESP_ERR_NO_MEM;
    }
    
    // Add network
    memcpy(&g_wifi_cfg->networks[g_wifi_cfg->network_count], network, sizeof(wifi_network_t));
    g_wifi_cfg->network_count++;
    
    wifi_cfg_nvs_save_networks(g_wifi_cfg->networks, g_wifi_cfg->network_count);
    wifi_cfg_unlock();
    
    wifi_cfg_event_post(WIFI_CFG_EVENT_NETWORK_ADDED, network, sizeof(wifi_network_t));
    ESP_LOGI(TAG, "Network added: %s", network->ssid);
    return ESP_OK;
}

esp_err_t wifi_cfg_update_network(const wifi_network_t *network)
{
    if (!g_wifi_cfg || !network) return ESP_ERR_INVALID_ARG;
    
    wifi_cfg_lock();
    
    for (size_t i = 0; i < g_wifi_cfg->network_count; i++) {
        if (strcmp(g_wifi_cfg->networks[i].ssid, network->ssid) == 0) {
            memcpy(&g_wifi_cfg->networks[i], network, sizeof(wifi_network_t));
            wifi_cfg_nvs_save_networks(g_wifi_cfg->networks, g_wifi_cfg->network_count);
            wifi_cfg_unlock();
            wifi_cfg_event_post(WIFI_CFG_EVENT_NETWORK_UPDATED, network, sizeof(wifi_network_t));
            ESP_LOGI(TAG, "Network updated: %s", network->ssid);
            return ESP_OK;
        }
    }
    
    wifi_cfg_unlock();
    return ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_cfg_remove_network(const char *ssid)
{
    if (!g_wifi_cfg || !ssid) return ESP_ERR_INVALID_ARG;
    
    wifi_cfg_lock();
    
    for (size_t i = 0; i < g_wifi_cfg->network_count; i++) {
        if (strcmp(g_wifi_cfg->networks[i].ssid, ssid) == 0) {
            for (size_t j = i; j < g_wifi_cfg->network_count - 1; j++) {
                g_wifi_cfg->networks[j] = g_wifi_cfg->networks[j + 1];
            }
            g_wifi_cfg->network_count--;
            wifi_cfg_nvs_save_networks(g_wifi_cfg->networks, g_wifi_cfg->network_count);
            wifi_cfg_unlock();
            wifi_cfg_event_post(WIFI_CFG_EVENT_NETWORK_REMOVED, ssid, strlen(ssid) + 1);
            ESP_LOGI(TAG, "Network removed: %s", ssid);
            return ESP_OK;
        }
    }
    
    wifi_cfg_unlock();
    return ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_cfg_get_network(const char *ssid, wifi_network_t *network)
{
    if (!g_wifi_cfg || !ssid || !network) return ESP_ERR_INVALID_ARG;
    
    wifi_cfg_lock();
    
    for (size_t i = 0; i < g_wifi_cfg->network_count; i++) {
        if (strcmp(g_wifi_cfg->networks[i].ssid, ssid) == 0) {
            memcpy(network, &g_wifi_cfg->networks[i], sizeof(wifi_network_t));
            wifi_cfg_unlock();
            return ESP_OK;
        }
    }
    
    wifi_cfg_unlock();
    return ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_cfg_list_networks(wifi_network_t *networks, size_t max_count, size_t *count)
{
    if (!g_wifi_cfg || !networks || !count) return ESP_ERR_INVALID_ARG;
    
    wifi_cfg_lock();
    
    size_t copy_count = g_wifi_cfg->network_count;
    if (copy_count > max_count) copy_count = max_count;
    
    memcpy(networks, g_wifi_cfg->networks, copy_count * sizeof(wifi_network_t));
    *count = copy_count;
    
    wifi_cfg_unlock();
    return ESP_OK;
}

// =============================================================================
// Connection API
// =============================================================================

esp_err_t wifi_cfg_connect(const char *ssid)
{
    if (!g_wifi_cfg) return ESP_ERR_INVALID_STATE;

    // Asking to connect lifts the suppression an explicit disconnect set.
    // Before this existed there was no way back: wifi_cfg_disconnect() cleared
    // config.auto_reconnect and nothing ever restored it.
    g_wifi_cfg->reconnect_suppressed = false;
    
    if (!ssid) {
        // Auto connect - send event to task
        wifi_cfg_send_event(WM_INT_EVT_START);
        return ESP_OK;
    }
    
    // Skip if already connected to the same SSID
    if (g_wifi_cfg->state == WIFI_STATE_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            if (strcmp((char *)ap_info.ssid, ssid) == 0) {
                ESP_LOGI(TAG, "Already connected to %s", ssid);
                return ESP_OK;
            }
        }
    }
    
    // Find network
    wifi_network_t *net = NULL;
    wifi_cfg_lock();
    for (size_t i = 0; i < g_wifi_cfg->network_count; i++) {
        if (strcmp(g_wifi_cfg->networks[i].ssid, ssid) == 0) {
            net = &g_wifi_cfg->networks[i];
            break;
        }
    }
    wifi_cfg_unlock();
    
    if (!net) return ESP_ERR_NOT_FOUND;
    
    ESP_LOGI(TAG, "Connecting to %s", ssid);
    
    g_wifi_cfg->state = WIFI_STATE_CONNECTING;
    wifi_cfg_event_post(WIFI_CFG_EVENT_CONNECTING, ssid, strlen(ssid) + 1);
    
    wifi_config_t wifi_cfg;
    fill_sta_config(&wifi_cfg, net);
    
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    return esp_wifi_connect();
}

esp_err_t wifi_cfg_disconnect(void)
{
    if (!g_wifi_cfg) return ESP_ERR_INVALID_STATE;
    
    // Suppress reconnection until something asks to connect again. Runtime
    // state, not config: the user's `auto_reconnect` intent is untouched, and
    // wifi_cfg_connect() lifts this.
    g_wifi_cfg->reconnect_suppressed = true;
    g_wifi_cfg->connecting = false;
    return esp_wifi_disconnect();
}

esp_err_t wifi_cfg_scan(wifi_scan_result_t *results, size_t max_count, size_t *count)
{
    if (!g_wifi_cfg || !results || !count) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Starting scan");

#if WIFI_CFG_C5_BLE_SCAN_WORKAROUND
    /*
     * ESP32-C5 cannot complete the 5 GHz part of an all-band Wi-Fi scan while
     * Bluetooth is running (Espressif IDFGH-15407). Both BLE provisioning
     * transports invoke this function from an active GATT connection, so the
     * failure also prevents the response from reaching the client. Restrict
     * only that scan to 2.4 GHz and restore the application's band mode after
     * collecting the results. A manually entered 5 GHz network remains usable
     * once the original AUTO/5G mode is restored.
     */
    wifi_band_mode_t saved_band_mode = WIFI_BAND_MODE_2G_ONLY;
    bool restore_band_mode = false;
    if (g_wifi_cfg->ble_active &&
        esp_wifi_get_band_mode(&saved_band_mode) == ESP_OK &&
        saved_band_mode != WIFI_BAND_MODE_2G_ONLY) {
        esp_err_t band_err = esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
        if (band_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to select 2.4 GHz for C5 BLE scan: %s",
                     esp_err_to_name(band_err));
            return band_err;
        }
        restore_band_mode = true;
        ESP_LOGW(TAG, "C5 BLE scan limited to 2.4 GHz (IDFGH-15407 workaround)");
    }
#endif

    // Stop connection attempt if in progress (scan not allowed while connecting)
    if (g_wifi_cfg->connecting || g_wifi_cfg->state == WIFI_STATE_CONNECTING) {
        ESP_LOGI(TAG, "Stopping connection for scan");
        g_wifi_cfg->connecting = false;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));  // Brief delay for disconnect
    }

    xEventGroupClearBits(g_wifi_cfg->event_group, WIFI_SCAN_DONE_BIT);

    wifi_scan_config_t scan_cfg = {
        .show_hidden = false,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, false);
    if (ret != ESP_OK) {
#if WIFI_CFG_C5_BLE_SCAN_WORKAROUND
        if (restore_band_mode) esp_wifi_set_band_mode(saved_band_mode);
#endif
        return ret;
    }
    
    EventBits_t bits = xEventGroupWaitBits(g_wifi_cfg->event_group,
                                           WIFI_SCAN_DONE_BIT | WIFI_STOPPING_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CFG_SCAN_TIMEOUT_MS));
    
    if (bits & WIFI_STOPPING_BIT) {
        esp_wifi_scan_stop();
#if WIFI_CFG_C5_BLE_SCAN_WORKAROUND
        if (restore_band_mode) esp_wifi_set_band_mode(saved_band_mode);
#endif
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupClearBits(g_wifi_cfg->event_group, WIFI_SCAN_DONE_BIT);

    if (!(bits & WIFI_SCAN_DONE_BIT)) {
        // A late scan-done event must not leak into the next request.
        esp_wifi_scan_stop();
#if WIFI_CFG_C5_BLE_SCAN_WORKAROUND
        if (restore_band_mode) esp_wifi_set_band_mode(saved_band_mode);
#endif
        return ESP_ERR_TIMEOUT;
    }
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    wifi_ap_record_t *ap_list = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (!ap_list) {
#if WIFI_CFG_C5_BLE_SCAN_WORKAROUND
        if (restore_band_mode) esp_wifi_set_band_mode(saved_band_mode);
#endif
        return ESP_ERR_NO_MEM;
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    
    size_t copy_count = ap_count;
    if (copy_count > max_count) copy_count = max_count;
    
    for (size_t i = 0; i < copy_count; i++) {
        strncpy(results[i].ssid, (char *)ap_list[i].ssid, sizeof(results[i].ssid) - 1);
        results[i].rssi = ap_list[i].rssi;
        results[i].auth = ap_list[i].authmode;
    }

    // Deduplicate by SSID, keeping the entry with strongest RSSI
    // This handles mesh networks and enterprise setups with multiple APs
    for (size_t i = 0; i < copy_count; i++) {
        for (size_t j = i + 1; j < copy_count; ) {
            if (strcmp(results[i].ssid, results[j].ssid) == 0) {
                // Keep stronger signal
                if (results[j].rssi > results[i].rssi) {
                    results[i].rssi = results[j].rssi;
                    results[i].auth = results[j].auth;
                }
                // Remove duplicate by shifting remaining entries
                memmove(&results[j], &results[j + 1], (copy_count - j - 1) * sizeof(wifi_scan_result_t));
                copy_count--;
            } else {
                j++;
            }
        }
    }

    *count = copy_count;
    free(ap_list);

#if WIFI_CFG_C5_BLE_SCAN_WORKAROUND
    if (restore_band_mode) {
        esp_err_t band_err = esp_wifi_set_band_mode(saved_band_mode);
        if (band_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restore C5 Wi-Fi band mode: %s",
                     esp_err_to_name(band_err));
            return band_err;
        }
    }
#endif
    
    uint16_t cnt = (uint16_t)copy_count;
    wifi_cfg_event_post(WIFI_CFG_EVENT_SCAN_DONE, &cnt, sizeof(cnt));
    
    ESP_LOGI(TAG, "Scan done, found %zu networks", copy_count);
    return ESP_OK;
}

// Human-readable auth mode, shared by the REST scan endpoint and the CLI
// `wifi scan` output. These strings are part of the REST API contract (the
// "auth" field of /api/scan) -- do not reword them.
const char *wifi_cfg_auth_str(wifi_auth_mode_t auth)
{
    switch (auth) {
        case WIFI_AUTH_OPEN:          return "OPEN";
        case WIFI_AUTH_WEP:           return "WEP";
        case WIFI_AUTH_WPA_PSK:       return "WPA";
        case WIFI_AUTH_WPA2_PSK:      return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:      return "WPA3";
        default:                      return "UNKNOWN";
    }
}
