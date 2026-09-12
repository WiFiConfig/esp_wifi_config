---
sidebar_position: 6
title: Examples
description: Complete example projects demonstrating different feature combinations
---

# Examples

The ESP-IDF examples are complete projects you can build and flash directly.
Each includes a `main.c`, `CMakeLists.txt`, `sdkconfig.defaults`, and
`idf_component.yml`. Native [Arduino sketches](#arduino-sketches) are also
available under `examples/arduino`.

```bash
cd examples/<example_name>
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## [basic](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/basic)

Minimal setup with default networks, REST API, and SoftAP captive portal. This is the best starting point — it shows event subscriptions, custom variables, and the provisioning-on-failure pattern with no optional features enabled.

---

## [with_cli](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_cli)

Adds serial console commands (`wifi status`, `wifi scan`, `wifi add`, etc.). Requires initializing the ESP Console REPL in your app code — the library registers its commands automatically when `CONFIG_WIFI_CFG_ENABLE_CLI=y`. Uses USB Serial JTAG console on ESP32-S3.

---

## [with_webui](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_webui)

Enables the embedded Preact Web UI (~10KB gzipped) with `CONFIG_WIFI_CFG_ENABLE_WEBUI=y`. No additional code needed — the Web UI is served automatically at the device's IP (or `192.168.4.1` in AP mode). Supports dark mode and captive portal auto-open.

---

## [with_webui_customize](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_webui_customize)

Serves a custom frontend from a LittleFS partition instead of the embedded UI. Requires a custom partition table with a 512 KB LittleFS partition and `CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH="/littlefs"`. With the custom path set, the embedded Preact assets are excluded from the build — your filesystem image must provide `index.html`, `assets/app.js` (or `.js.gz`), and `assets/index.css` (or `.css.gz`).

See the [Custom Web UI guide](./guides/custom-webui.md) for the full workflow (partition table, Vite config, gzip handling, captive-portal interaction).

---

## [with_ble](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_ble)

BLE provisioning using ESP-IDF's official Wi-Fi Provisioning manager
(NimBLE host, Security 1 + Proof-of-Possession). Provision via the
"ESP BLE Provisioning" mobile app (iOS / Android), the `esp_prov`
Python tool, or any client built on the
`esp-idf-provisioning-{android,ios}` SDKs. Requires
`CONFIG_BT_ENABLED=y`, a NimBLE or Bluedroid host, and
`CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` (BLE adds ~100 KB flash).

---

## [with_ble_deinit](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_ble_deinit)

Demonstrates app-owned BLE stack with NimBLE using the **Improv BLE**
host bootstrap. The app initialises NimBLE first, then WiFi Config
registers only its Improv GATT service (service-only mode). After
provisioning, `wifi_cfg_deinit()` removes the WiFi Config service
while NimBLE stays running for the app's own BLE services. Network
Provisioning BLE manages its own host lifecycle and isn't suitable for
this particular handoff pattern.

---

## [with_improv](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_improv)

Improv WiFi standard provisioning over BLE and (optionally) Serial.
Supports Web Bluetooth (Chrome/Edge at [improv-wifi.com](https://www.improv-wifi.com/)),
the ESPHome companion app, and Web Serial. Mutually exclusive with
Network Provisioning BLE — pick one BLE protocol per firmware build.
Improv Serial is independent of BLE and remains safe alongside Network
Provisioning.

---

## Arduino sketches

Use Arduino-ESP32 **3.3.11** and ESP WiFi Config **0.3.1**. Follow the
[Arduino guide](./arduino.md) for installation and partition selection.

| Sketch in `examples/arduino/` | Demonstrates |
| --- | --- |
| `Basic` | SoftAP captive portal and saved-network management |
| `ProvisioningBLE` | Espressif provisioning using the stock BLE host |
| `ImprovBLE` | Improv provisioning using the stock BLE host |
| `NimBLEProvisioning` | Espressif provisioning using NimBLE-Arduino |
| `NimBLEImprov` | Improv provisioning using NimBLE-Arduino |
| `ImprovSerial` | Serial provisioning through an Arduino `Stream` |
| `NoPortal` | Building with SoftAP and the web UI disabled |
| `Client` / `Lifecycle` | Arduino network clients and repeated initialization/shutdown |

The two NimBLE sketches require **NimBLE-Arduino 2.5.1** and include their
`build_opt.h` flags. Both BLE protocols are alternatives; choose the one your
client supports. The `PlatformIO` example includes a `nimble` environment with
the optional dependency and flags configured. See the
[resource comparison](./arduino.md#nimble-arduino-measurements) before selecting
a BLE host for your board.
