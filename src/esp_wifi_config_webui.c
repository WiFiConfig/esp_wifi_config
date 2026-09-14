/**
 * @file esp_wifi_config_webui.c
 * @brief Web UI serving for WiFi Config
 *
 * The library owns the routes ("/" and the slash-star wildcard, registered for the
 * lifetime of provisioning by esp_wifi_config_http.c); this file decides
 * where the bytes come from. The chain for every GET is:
 *
 *   1. application asset provider (wifi_cfg_webui_set_asset_provider())
 *   2. the compiled-in source selected by CONFIG_WIFI_CFG_WEBUI_SOURCE_*:
 *        EMBEDDED    the library's frontend/dist, linked into the firmware
 *        FILESYSTEM  files under CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH
 *        APPLICATION nothing -- the provider is the only source
 *   3. 404
 *
 * Every mode decision here is made from CONFIG_* symbols so the same source
 * builds identically under idf.py, PlatformIO and Arduino. The one CMake-fed
 * define, WIFI_CFG_WEBUI_EMBED_FILES, only says *how* the embedded bytes
 * arrived (EMBED_FILES linker symbols vs. the generated C header).
 */

#include "esp_wifi_config_priv.h"
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>

#ifdef CONFIG_WIFI_CFG_ENABLE_WEBUI

static const char *TAG = "wifi_cfg_webui";

// =============================================================================
// Application asset provider
// =============================================================================

/* File-level statics rather than g_wifi_cfg: the application may register
 * before wifi_cfg_init(), and the registration has to survive provisioning
 * stop / restart (which re-registers the URI handlers but not this). */
static wifi_cfg_webui_asset_provider_t s_asset_provider = NULL;
static void *s_asset_provider_ctx = NULL;

esp_err_t wifi_cfg_webui_set_asset_provider(wifi_cfg_webui_asset_provider_t provider, void *ctx)
{
    s_asset_provider = provider;
    s_asset_provider_ctx = provider ? ctx : NULL;
    ESP_LOGI(TAG, "Asset provider %s", provider ? "registered" : "cleared");
    return ESP_OK;
}

/**
 * @brief Determine MIME content type from file path
 */
static const char *get_content_type(const char *filepath)
{
    if (strstr(filepath, ".html")) return "text/html";
    if (strstr(filepath, ".js"))   return "application/javascript";
    if (strstr(filepath, ".css"))  return "text/css";
    if (strstr(filepath, ".json")) return "application/json";
    if (strstr(filepath, ".png"))  return "image/png";
    if (strstr(filepath, ".svg"))  return "image/svg+xml";
    if (strstr(filepath, ".ico"))  return "image/x-icon";
    if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) return "image/jpeg";
    if (strstr(filepath, ".woff2")) return "font/woff2";
    if (strstr(filepath, ".woff")) return "font/woff";
    if (strstr(filepath, ".ttf"))  return "font/ttf";
    if (strstr(filepath, ".gif"))  return "image/gif";
    if (strstr(filepath, ".webp")) return "image/webp";
    return "text/plain";
}

static bool serve_from_provider(httpd_req_t *req, const char *filepath)
{
    if (!s_asset_provider) {
        return false;
    }

    wifi_cfg_webui_asset_t asset = { 0 };
    if (!s_asset_provider(filepath, &asset, s_asset_provider_ctx)) {
        return false;
    }
    if (!asset.data) {
        ESP_LOGW(TAG, "Asset provider accepted %s but returned no data", filepath);
        return false;
    }

    httpd_resp_set_type(req, asset.content_type ? asset.content_type : get_content_type(filepath));
    if (asset.gzipped) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }
    httpd_resp_send(req, (const char *)asset.data, asset.len);
    ESP_LOGD(TAG, "Served from application provider: %s", filepath);
    return true;
}

// =============================================================================
// Filesystem source
// =============================================================================

#ifdef CONFIG_WIFI_CFG_WEBUI_SOURCE_FILESYSTEM

static const char *get_fs_base_path(void)
{
    return CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH;
}

/**
 * @brief Try to serve file from the configured filesystem path (SPIFFS/LittleFS)
 */
static bool serve_from_filesystem(httpd_req_t *req, const char *filepath)
{
    const char *base_path = get_fs_base_path();

    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "%s%s", base_path, filepath);

    struct stat st;
    bool gzipped = false;
    char gzpath[132];
    snprintf(gzpath, sizeof(gzpath), "%s.gz", fullpath);

    if (stat(fullpath, &st) != 0) {
        // No uncompressed file — try gzipped variant
        if (stat(gzpath, &st) != 0) {
            return false;
        }
        gzipped = true;
    } else if (stat(gzpath, &st) == 0) {
        // Both exist — prefer gzipped
        gzipped = true;
    }

    FILE *f = fopen(gzipped ? gzpath : fullpath, "r");
    if (!f) {
        return false;
    }

    httpd_resp_set_type(req, get_content_type(filepath));
    if (gzipped) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    // Stream file
    char buf[512];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }
    httpd_resp_send_chunk(req, NULL, 0);

    fclose(f);
    ESP_LOGD(TAG, "Served from filesystem: %s", filepath);
    return true;
}

#endif // CONFIG_WIFI_CFG_WEBUI_SOURCE_FILESYSTEM

// =============================================================================
// Embedded source (the library's own frontend)
// =============================================================================

#ifdef CONFIG_WIFI_CFG_WEBUI_SOURCE_EMBEDDED

#ifdef WIFI_CFG_WEBUI_EMBED_FILES
// Component CMake put frontend/dist in EMBED_FILES: use the linker symbols.
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_js_gz_start[] asm("_binary_app_js_gz_start");
extern const uint8_t app_js_gz_end[] asm("_binary_app_js_gz_end");
extern const uint8_t index_css_gz_start[] asm("_binary_index_css_gz_start");
extern const uint8_t index_css_gz_end[] asm("_binary_index_css_gz_end");
#else
// No component CMake ran (Arduino, PlatformIO): the same bytes as a C array,
// generated from frontend/dist by tools/generate_arduino_assets.py.
#include "arduino/webui_assets.h"
#endif

static const struct {
    const char *uri;
    const uint8_t *start;
    const uint8_t *end;
    bool gzipped;
} embedded_assets[] = {
    { "/index.html",       index_html_start,   index_html_end,   false },
    { "/assets/app.js",    app_js_gz_start,    app_js_gz_end,    true  },
    { "/assets/index.css", index_css_gz_start, index_css_gz_end, true  },
};

static bool serve_embedded(httpd_req_t *req, const char *filepath)
{
    for (size_t i = 0; i < sizeof(embedded_assets) / sizeof(embedded_assets[0]); i++) {
        if (strcmp(filepath, embedded_assets[i].uri) == 0) {
            httpd_resp_set_type(req, get_content_type(filepath));
            if (embedded_assets[i].gzipped) {
                httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            }
            httpd_resp_send(req, (const char *)embedded_assets[i].start,
                            embedded_assets[i].end - embedded_assets[i].start);
            return true;
        }
    }
    return false;
}

#endif // CONFIG_WIFI_CFG_WEBUI_SOURCE_EMBEDDED

// =============================================================================
// Handler
// =============================================================================

/**
 * @brief Unified handler for all Web UI static files
 *
 * Application provider first, then the compiled-in source. Remaps "/" to
 * "/index.html".
 */
static esp_err_t handler_webui_static(httpd_req_t *req)
{
    const char *filepath = req->uri;

    if (strcmp(filepath, "/") == 0) {
        filepath = "/index.html";
    }

    if (serve_from_provider(req, filepath)) {
        return ESP_OK;
    }

#ifdef CONFIG_WIFI_CFG_WEBUI_SOURCE_FILESYSTEM
    if (serve_from_filesystem(req, filepath)) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "404 Not Found: uri=%s (tried %s%s)", req->uri, get_fs_base_path(), filepath);
#elif defined(CONFIG_WIFI_CFG_WEBUI_SOURCE_EMBEDDED)
    if (serve_embedded(req, filepath)) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "404 Not Found: uri=%s (not an embedded asset)", req->uri);
#else
    ESP_LOGW(TAG, "404 Not Found: uri=%s (%s)", req->uri,
             s_asset_provider ? "application provider declined"
                              : "no asset provider registered; see wifi_cfg_webui_set_asset_provider()");
#endif

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
    return ESP_FAIL;
}

/**
 * @brief Initialize Web UI handlers
 */
esp_err_t wifi_cfg_webui_init(httpd_handle_t httpd)
{
    if (!httpd) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler_webui_static,
    };
    httpd_register_uri_handler(httpd, &index_uri);

    httpd_uri_t wildcard_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = handler_webui_static,
    };
    httpd_register_uri_handler(httpd, &wildcard_uri);

#ifdef CONFIG_WIFI_CFG_WEBUI_SOURCE_EMBEDDED
    size_t total_size = (index_html_end - index_html_start) +
                        (app_js_gz_end - app_js_gz_start) +
                        (index_css_gz_end - index_css_gz_start);
    ESP_LOGI(TAG, "Web UI registered (source: embedded, %zu bytes%s)", total_size,
             s_asset_provider ? ", application provider first" : "");
#elif defined(CONFIG_WIFI_CFG_WEBUI_SOURCE_FILESYSTEM)
    ESP_LOGI(TAG, "Web UI registered (source: filesystem %s%s)", get_fs_base_path(),
             s_asset_provider ? ", application provider first" : "");
#else
    if (s_asset_provider) {
        ESP_LOGI(TAG, "Web UI registered (source: application provider)");
    } else {
        ESP_LOGW(TAG, "Web UI registered with CONFIG_WIFI_CFG_WEBUI_SOURCE_APPLICATION but no "
                      "asset provider is registered; every page will 404 until "
                      "wifi_cfg_webui_set_asset_provider() is called");
    }
#endif

#if defined(CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH) && !defined(CONFIG_WIFI_CFG_WEBUI_SOURCE_FILESYSTEM)
    /* Kconfig keeps the path visible in every mode so a stale one can be
     * reported. The component CMake refuses to build in this state; this
     * catches builds that never ran it (PlatformIO). */
    if (CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH[0] != '\0') {
        ESP_LOGW(TAG, "CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH=\"%s\" is ignored: the Web UI source is not "
                      "the filesystem (select CONFIG_WIFI_CFG_WEBUI_SOURCE_FILESYSTEM=y to use it)",
                 CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH);
    }
#endif

    return ESP_OK;
}

#else // !CONFIG_WIFI_CFG_ENABLE_WEBUI

esp_err_t wifi_cfg_webui_init(httpd_handle_t httpd)
{
    (void)httpd;
    return ESP_OK;
}

esp_err_t wifi_cfg_webui_set_asset_provider(wifi_cfg_webui_asset_provider_t provider, void *ctx)
{
    (void)provider;
    (void)ctx;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif // CONFIG_WIFI_CFG_ENABLE_WEBUI
