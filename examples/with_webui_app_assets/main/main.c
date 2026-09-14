/**
 * @file main.c
 * @brief ESP WiFi Config - Application-Provided Web UI Example
 *
 * This example demonstrates:
 * - A custom Web UI embedded in the firmware image by the *application*
 *   (main/CMakeLists.txt EMBED_FILES), not by the library
 * - Handing those bytes to the library with wifi_cfg_webui_set_asset_provider()
 * - CONFIG_WIFI_CFG_WEBUI_SOURCE_APPLICATION=y, so the library links in no
 *   frontend of its own and touches no filesystem
 *
 * The library still owns the routes, the captive-portal redirects and the
 * provisioning start/stop lifecycle. Because the UI lives in the app image,
 * an OTA update carries it along -- there is no separate partition to keep
 * in sync.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi_config.h"

static const char *TAG = "wifi_webui_app";

// -----------------------------------------------------------------------------
// Embedded assets. The symbol names come from the file names listed in
// EMBED_FILES: "index.html" -> _binary_index_html_start, and so on.
// -----------------------------------------------------------------------------
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t app_js_start[]     asm("_binary_app_js_start");
extern const uint8_t app_js_end[]       asm("_binary_app_js_end");
extern const uint8_t index_css_start[]  asm("_binary_index_css_start");
extern const uint8_t index_css_end[]    asm("_binary_index_css_end");

typedef struct {
    const char *path;       // request path as the library sees it
    const uint8_t *start;
    const uint8_t *end;
    bool gzipped;           // true for files embedded as .gz
} asset_entry_t;

/* To ship compressed assets instead, embed the .gz files
 * ("../www/assets/app.js.gz" -> _binary_app_js_gz_start) and set gzipped to
 * true. The request path stays "/assets/app.js"; the library adds the
 * Content-Encoding header. */
static const asset_entry_t s_assets[] = {
    { "/index.html",       index_html_start, index_html_end, false },
    { "/assets/app.js",    app_js_start,     app_js_end,     false },
    { "/assets/index.css", index_css_start,  index_css_end,  false },
};

/**
 * @brief Asset provider: answer the library's request for a path
 *
 * Runs on the HTTP server task. Return false for anything not ours and the
 * library answers 404 (there is no other source in APPLICATION mode).
 */
static bool provide_asset(const char *path, wifi_cfg_webui_asset_t *out, void *ctx)
{
    (void)ctx;
    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); i++) {
        if (strcmp(path, s_assets[i].path) == 0) {
            out->data = s_assets[i].start;
            out->len = s_assets[i].end - s_assets[i].start;
            out->gzipped = s_assets[i].gzipped;
            out->content_type = NULL;   // let the library infer from the extension
            return true;
        }
    }
    return false;
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    if (event_id == WIFI_CFG_EVENT_CONNECTED) {
        const wifi_connected_t *info = (const wifi_connected_t *)data;
        ESP_LOGI(TAG, "Connected to %s", info->ssid);
    } else if (event_id == WIFI_CFG_EVENT_GOT_IP) {
        wifi_status_t status;
        if (wifi_cfg_get_status(&status) == ESP_OK) {
            ESP_LOGI(TAG, "Got IP: %s", status.ip);
            ESP_LOGI(TAG, "Web UI: http://%s/", status.ip);
        }
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "  ESP WiFi Config - Application-Provided Web UI");
    ESP_LOGI(TAG, "==============================================");

    // Register the provider before init so it is in place the moment the
    // portal comes up. It survives provisioning stop / restart.
    ESP_ERROR_CHECK(wifi_cfg_webui_set_asset_provider(provide_asset, NULL));

    size_t total = 0;
    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); i++) {
        total += s_assets[i].end - s_assets[i].start;
    }
    ESP_LOGI(TAG, "Embedded Web UI: %zu bytes in %zu files", total,
             sizeof(s_assets) / sizeof(s_assets[0]));

    // The default event loop must exist before registering handlers.
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_event_handler_register(WIFI_CFG_EVENT, WIFI_CFG_EVENT_CONNECTED, on_wifi_event, NULL);
    esp_event_handler_register(WIFI_CFG_EVENT, WIFI_CFG_EVENT_GOT_IP, on_wifi_event, NULL);

    wifi_cfg_config_t config = {
        WIFI_CFG_DEFAULTS,   // required: init no longer patches unset fields
        .stop_provisioning_on_connect = true,
        .provisioning_teardown_delay_ms = 5000,
        .enable_ap = true,
    };
    snprintf(config.default_ap.ssid, sizeof(config.default_ap.ssid), "ESP32-App-{id}");

    ret = wifi_cfg_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WiFi Config: %s", esp_err_to_name(ret));
        return;
    }

    ret = wifi_cfg_wait_connected(30000);
    if (ret == ESP_OK) {
        wifi_status_t status;
        wifi_cfg_get_status(&status);
        ESP_LOGI(TAG, "Connected! Web UI at http://%s/", status.ip);
    } else {
        ESP_LOGW(TAG, "No saved networks. Connect to AP: ESP32-App-XXXXXX");
        ESP_LOGI(TAG, "Then open http://192.168.4.1/");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
