---
sidebar_position: 5
title: Custom Web UI
description: Ship your own captive-portal frontend, embedded in your firmware or on LittleFS / SPIFFS, instead of the bundled Preact UI
---

# Custom Web UI

The library serves a captive-portal frontend at `/`. Where the files come
from is yours to choose: the bundled Preact UI, files your application
embeds in its own firmware image, or a filesystem partition (LittleFS or
SPIFFS). The routes, the captive-portal redirects and the provisioning
start/stop lifecycle are the same in every case — only the byte source
changes.

## Serving Modes

The Web UI source is a Kconfig `choice`, `CONFIG_WIFI_CFG_WEBUI_SOURCE`.
Exactly one is selected:

| Mode | Kconfig | Source of files |
|---|---|---|
| **Embedded** (default) | `WIFI_CFG_ENABLE_WEBUI=y`<br/>`WIFI_CFG_WEBUI_SOURCE_EMBEDDED=y` | Bundled Preact app (~16 KB gzipped) linked into the firmware by the library |
| **Application** | `WIFI_CFG_ENABLE_WEBUI=y`<br/>`WIFI_CFG_WEBUI_SOURCE_APPLICATION=y` | Assets your application embeds and hands over through `wifi_cfg_webui_set_asset_provider()` |
| **Filesystem** | `WIFI_CFG_ENABLE_WEBUI=y`<br/>`WIFI_CFG_WEBUI_SOURCE_FILESYSTEM=y`<br/>`WIFI_CFG_WEBUI_CUSTOM_PATH="/littlefs"` | Files on the configured filesystem path |
| **Simple fallback page** | `WIFI_CFG_ENABLE_WEBUI=n` | A built-in minimal HTML page with inline JS, just enough to add a network and connect |

Which one to pick:

- **Application** when the UI is yours and should live in the firmware
  image: it ships with every OTA, needs no extra partition, and works the
  same under `idf.py` and PlatformIO. This is the mode for a product that
  builds on the library's frontend (see `frontend/src/lib.ts`) or replaces
  it.
- **Filesystem** when the UI should be replaceable without reflashing
  firmware, or is too large to sit in the app partition.
- **Embedded** when the bundled UI is what you want.

:::caution The compiled-in source is the only source
Under the application and filesystem modes the bundled Preact assets are
**excluded from the build entirely**. A path the provider declines, or a
file missing from the filesystem image, is a 404 — there is no runtime
fallback to the embedded content.
:::

### The asset provider

`wifi_cfg_webui_set_asset_provider()` is a runtime registration and is
available in every mode. Under the application source it is the only
source. Under the embedded or filesystem source it is consulted *first*,
so an application can override a single file (say, `/index.html` with
its own branding) while everything else comes from the configured
source.

## Required File Layout

The bundled `index.html` references exactly two assets, and the library's
embedded table knows about exactly these three paths:

| URL | File | Notes |
|---|---|---|
| `/` | `index.html` | Main HTML document. `/` is internally remapped to `/index.html` before any source is asked. |
| `/assets/app.js` | `assets/app.js` or `assets/app.js.gz` | JS bundle. Single-file output required (no code splitting). |
| `/assets/index.css` | `assets/index.css` or `assets/index.css.gz` | CSS stylesheet. |

A custom UI is not limited to these. The server registers a `/*` wildcard
handler, so **any path** your provider answers, or any file under
`WEBUI_CUSTOM_PATH` (fonts, images, extra chunks), is served too. For the
filesystem source, paths longer than ~120 characters including the base
path are not served.

### Gzip handling

The server checks for both the plain file and a `.gz` sibling for every
request:

- If only the plain file exists → serve plain.
- If only the gzipped file exists → serve gzipped, add `Content-Encoding: gzip`.
- If **both** exist → prefer the gzipped variant (smaller response, same `Content-Type`).

Gzipping is recommended for `app.js` and `index.css` — the embedded
Preact build ships both as `.js.gz` and `.css.gz` and saves around 70%
on the wire.

### Content types

The server auto-sets `Content-Type` from the file extension. Supported
extensions: `.html`, `.css`, `.js`, `.json`, `.svg`, `.png`. Unknown
extensions fall back to `application/octet-stream`.

## Minimal HTML Template

A custom frontend can use any framework or none at all. The only hard
requirements are:

- Mount JS at `/assets/app.js`.
- Mount CSS at `/assets/index.css`.
- Output a single JS bundle (no dynamic imports / code splitting).

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>My Device Setup</title>
  <link rel="stylesheet" href="/assets/index.css">
  <script type="module" crossorigin src="/assets/app.js"></script>
</head>
<body>
  <div id="app"></div>
</body>
</html>
```

The DOM structure inside `<body>` is entirely up to you — the library
serves the files but does not require any particular markup.

## Vite Build Configuration

If your frontend uses Vite (the bundled Preact app does), the relevant
rollup output settings are:

```typescript
// vite.config.ts
import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';
import { compression } from 'vite-plugin-compression2';

export default defineConfig({
  plugins: [
    preact(),
    compression({ algorithm: 'gzip' }),  // emit .gz alongside originals
  ],
  build: {
    rollupOptions: {
      output: {
        inlineDynamicImports: true,      // force single JS bundle
        entryFileNames: 'assets/app.js', // fixed filename, no hash
        assetFileNames: 'assets/[name].[ext]',  // index.css (no hash)
      },
    },
  },
});
```

The non-negotiable bits are:

- **`inlineDynamicImports: true`** — the embedded build has exactly three
  fixed assets, so dynamic chunks would be unreachable there. A filesystem
  build can serve extra files, but single-file output keeps the two paths
  interchangeable.
- **Fixed `app.js` / `index.css` output names** — no content hashes, since
  `index.html` and the embedded asset table reference those exact URLs.

The bundled frontend lives at [`frontend/`](https://github.com/WiFiConfig/esp_wifi_config/tree/main/frontend);
its `vite.config.ts` is the canonical reference.

## Deployment: Embedded in Your Firmware (Application Source)

A complete worked example lives at
[`examples/with_webui_app_assets/`](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_webui_app_assets).
The moving parts:

### 1. sdkconfig

```
CONFIG_WIFI_CFG_ENABLE_WEBUI=y
CONFIG_WIFI_CFG_WEBUI_SOURCE_APPLICATION=y
```

The library now links in no frontend of its own.

### 2. Embed the files

ESP-IDF, in your main component's `CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES
                        "../www/index.html"
                        "../www/assets/app.js.gz"
                        "../www/assets/index.css.gz")
```

PlatformIO, in `platformio.ini`:

```ini
board_build.embed_files =
    www/index.html
    www/assets/app.js.gz
    www/assets/index.css.gz
```

Both produce `_binary_<name>_start` / `_binary_<name>_end` linker
symbols, with dots and slashes in the file name replaced by underscores.

### 3. Register a provider

```c
#include "esp_wifi_config.h"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t app_js_gz_start[]  asm("_binary_app_js_gz_start");
extern const uint8_t app_js_gz_end[]    asm("_binary_app_js_gz_end");
extern const uint8_t index_css_gz_start[] asm("_binary_index_css_gz_start");
extern const uint8_t index_css_gz_end[]   asm("_binary_index_css_gz_end");

static const struct {
    const char *path;
    const uint8_t *start, *end;
    bool gzipped;
} assets[] = {
    { "/index.html",       index_html_start,   index_html_end,   false },
    { "/assets/app.js",    app_js_gz_start,    app_js_gz_end,    true  },
    { "/assets/index.css", index_css_gz_start, index_css_gz_end, true  },
};

static bool provide_asset(const char *path, wifi_cfg_webui_asset_t *out, void *ctx)
{
    for (size_t i = 0; i < sizeof(assets) / sizeof(assets[0]); i++) {
        if (strcmp(path, assets[i].path) == 0) {
            out->data    = assets[i].start;
            out->len     = assets[i].end - assets[i].start;
            out->gzipped = assets[i].gzipped;
            return true;               // content_type NULL: inferred from extension
        }
    }
    return false;                      // 404
}

void app_main(void)
{
    wifi_cfg_webui_set_asset_provider(provide_asset, NULL);
    // ... wifi_cfg_init(&config);
}
```

The provider runs on the HTTP server task. The bytes must stay valid for
the life of the firmware (the library sends straight from `data` and never
copies), which flash-resident arrays satisfy by construction. Register it
before `wifi_cfg_init()` so it is in place the moment the portal comes
up; the registration survives provisioning stop and restart.

Set `out->content_type` to override the MIME type the library infers from
the extension. Pass `NULL` as the provider to clear it.

## Deployment: Custom Frontend on LittleFS (Filesystem Source)

A complete worked example lives at
[`examples/with_webui_customize/`](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_webui_customize)
— this section summarises the moving parts.

### 1. sdkconfig

```
CONFIG_WIFI_CFG_ENABLE_WEBUI=y
CONFIG_WIFI_CFG_WEBUI_SOURCE_FILESYSTEM=y
CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH="/littlefs"
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

### 2. Partition table

Add a LittleFS data partition (here 512 KB at the tail of flash):

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
storage,  data, littlefs,,        512K,
```

Adjust `factory` size for your firmware and pick a `storage` size that
comfortably fits your assets (compressed). The bundled Preact UI is
~16 KB gzipped, but a richer custom app can easily reach 100–200 KB.

### 3. Place frontend output

```
www/
├── index.html
└── assets/
    ├── app.js.gz
    └── index.css.gz
```

### 4. Wire LittleFS into CMake

In the project's top-level `CMakeLists.txt`:

```cmake
littlefs_create_partition_image(storage www FLASH_IN_PROJECT)
```

`storage` must match the partition name from `partitions.csv`. The
component `joltwallet/littlefs` (or the in-tree LittleFS port) needs to
be in `idf_component.yml` for this CMake function to exist.

### 5. Build and flash

```bash
idf.py build flash
```

Your application mounts the partition (see `init_littlefs()` in the
example's `main.c`) before starting WiFi Config. The library then serves
`/littlefs/...` for any GET request the API does not handle.

## Iterating on the Frontend

Because the filesystem image is independent of the firmware binary,
frontend-only changes don't require rebuilding firmware. After editing
your frontend:

```bash
# Rebuild frontend → www/
npm --prefix frontend run build

# Re-flash just the LittleFS partition
idf.py littlefs-flash
```

(The exact target name depends on your LittleFS component; some expose
`storage-flash` or similar.)

## Captive-Portal Behaviour

Captive-portal detection probes are handled regardless of which Web UI
mode is active:

| Path | Platform |
|---|---|
| `/generate_204`, `/gen_204` | Android |
| `/hotspot-detect.html`, `/library/test/success.html` | iOS / macOS |
| `/ncsi.txt`, `/connecttest.txt` | Windows |
| `/success.txt`, `/canonical.html` | Firefox |

All probes return an HTTP 302 redirect to `http://<AP_IP>/`, which the
phone/laptop then renders inside its captive-portal popup. This works
whether `/` is served from the embedded UI, your application's assets,
your filesystem UI, or the simple fallback page.

## Talking to the Backend

The custom frontend communicates with the device over the REST API
documented in [REST API Reference](../api/rest-api). Base path defaults
to `/api/wifi` and is configurable through `wifi_cfg_http_config_t`:

```c
.http = {
    .api_base_path = "/api/wifi",   // default
    .enable_auth   = false,         // set true for HTTP Basic Auth
    .pre_request_hook = my_hook,    // optional; see HTTP Server Sharing
},
```

If you need to host other endpoints alongside the WiFi Config API on
the same server, see [HTTP Server Sharing](./http-server-sharing).

## Reference Examples

- [`examples/with_webui_app_assets/`](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_webui_app_assets)
  — a small vanilla-JS UI embedded in the application firmware with
  `EMBED_FILES` and served through an asset provider.
- [`examples/with_webui_customize/`](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_webui_customize)
  — copies the bundled Preact frontend into the example's `www/`
  directory and flashes both firmware and LittleFS image from a single
  `idf.py build flash`.
