# WiFi Config - Application-Provided Web UI Example

Example demonstrating a **custom Web UI embedded in the firmware by the
application**, served through WiFi Config.

## Features

- The application owns the Web UI bytes (`main/CMakeLists.txt` `EMBED_FILES`)
- `CONFIG_WIFI_CFG_WEBUI_SOURCE_APPLICATION=y`: the library links in no
  frontend of its own and touches no filesystem
- `wifi_cfg_webui_set_asset_provider()` hands the bytes to the library on request
- The library keeps owning `/`, the `/*` wildcard, the captive-portal
  redirects and the provisioning start/stop lifecycle
- No filesystem partition: the UI ships inside the app image, so every OTA
  update carries it

This is the mode for a product that customises the Web UI (its own branding,
extra setup steps, or a completely different frontend) and wants it in flash
alongside the firmware rather than on LittleFS.

## How It Works

1. `main/CMakeLists.txt` lists the files under `www/` in `EMBED_FILES`. The
   build turns each into a pair of linker symbols,
   `_binary_<name>_start` / `_binary_<name>_end`, with dots and slashes in
   the file name replaced by underscores.
2. `main.c` declares those symbols and maps request paths to them in a small
   table.
3. `provide_asset()` is registered with `wifi_cfg_webui_set_asset_provider()`
   before `wifi_cfg_init()`. The library calls it for every GET that is not
   an API route; it returns `true` with the bytes, or `false` for a 404.

Under PlatformIO the same thing is `board_build.embed_files` in
`platformio.ini`; it produces identical symbol names.

## Build & Flash

```bash
cd examples/with_webui_app_assets
idf.py build flash monitor
```

Connect to the `ESP32-App-XXXXXX` access point and open `http://192.168.4.1/`.

## File Structure

```
www/
├── index.html          # served for / and /index.html
└── assets/
    ├── app.js          # vanilla JS against the REST API
    └── index.css
```

The three paths mirror what the library's own frontend uses, but that is a
convention, not a requirement. Any path the provider answers is served; the
only fixed point is that `/` is remapped to `/index.html` before the provider
is asked.

## Shipping Gzipped Assets

Gzip the files at build time (or check in the `.gz`), embed those instead,
and set `gzipped = true` in the table:

```cmake
EMBED_FILES "../www/index.html" "../www/assets/app.js.gz" "../www/assets/index.css.gz"
```

```c
extern const uint8_t app_js_gz_start[] asm("_binary_app_js_gz_start");
extern const uint8_t app_js_gz_end[]   asm("_binary_app_js_gz_end");
// ...
{ "/assets/app.js", app_js_gz_start, app_js_gz_end, true },
```

The request path stays `/assets/app.js`; the library adds
`Content-Encoding: gzip`. The library's own frontend ships this way at
~16 KB total.

## Using the Library's Frontend as a Base

The bundled Preact frontend exports its API client, stores and components
(`frontend/src/lib.ts`) so a product UI can extend it rather than fork it.
Build that UI with Vite into `www/` and embed the output exactly as above.
See `frontend/README.md` in the library for the Vite alias setup.

## REST API Reference

The frontend talks to the device over the REST API; see
[`website/docs/api/rest-api.md`](../../website/docs/api/rest-api.md).
