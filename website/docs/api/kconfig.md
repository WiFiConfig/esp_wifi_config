---
sidebar_position: 5
title: Kconfig Options
description: Compile-time configuration options via menuconfig
---

# Kconfig Options

Configure via `idf.py menuconfig` → WiFi Config, or set in `sdkconfig.defaults`.

:::warning An assignment whose dependency is unmet is dropped silently
Several options below `depend on` another option. If you set one in
`sdkconfig.defaults` while its dependency is off, kconfig discards the
line **without a warning** and the build succeeds having configured
nothing — the symbol does not even appear in the generated `sdkconfig`.
The pairs that bite are `ENABLE_IMPROV_SERIAL` (needs `ENABLE_CLI` off)
and `ENABLE_WEBUI` (needs `ENABLE_SOFTAP` on). After changing a
fragment, confirm the symbol survived:

```bash
grep WIFI_CFG_ENABLE_IMPROV_SERIAL build/../sdkconfig
```
:::

## Core Options

| Option | Default | Description |
|---|---|---|
| `CONFIG_WIFI_CFG_MAX_NETWORKS` | 5 | Maximum number of saved networks |
| `CONFIG_WIFI_CFG_MAX_VARS` | 10 | Maximum number of custom variables |
| `CONFIG_WIFI_CFG_DEFAULT_RETRY` | 3 | Retries per network before moving to next |
| `CONFIG_WIFI_CFG_RETRY_INTERVAL_MS` | 5000 | Base retry interval in milliseconds |
| `CONFIG_WIFI_CFG_MAX_SCAN_RESULTS` | 20 | Maximum scan results returned by the scan API |
| `CONFIG_WIFI_CFG_HTTP_MAX_CONTENT_LEN` | 2048 | Maximum HTTP request body size in bytes |
| `CONFIG_WIFI_CFG_TASK_STACK_SIZE` | 4096 | Stack size in bytes for the WiFi Config task |
| `CONFIG_WIFI_CFG_TASK_PRIORITY` | 5 | FreeRTOS priority for the WiFi Config and DNS tasks |
| `CONFIG_WIFI_CFG_HTTP_MAX_URI_HANDLERS` | 32 | Max URI handlers registered with the HTTP server — API(18) + WebUI(3) + captive(8) + reserve |

## SoftAP Provisioning Portal

| Option | Default | Description |
|---|---|---|
| `CONFIG_WIFI_CFG_ENABLE_SOFTAP` | y | Build the SoftAP portal: the access point, the HTTP server and its REST API, the eight captive-portal handlers and the captive DNS responder |

Default `y`, matching the behaviour before the option existed. Set it to
`n` only when the device is provisioned exclusively over Improv Serial,
Improv BLE or Network Provisioning BLE — it saves roughly 20–28 KB of
flash depending on which transport remains, because dropping the last
user of the HTTP server takes cJSON and newlib's floating-point string
conversion with it.

With it off, `wifi_cfg_start_ap()`, `wifi_cfg_stop_ap()`,
`wifi_cfg_get_ap_status()`, `wifi_cfg_set_ap_config()`,
`wifi_cfg_get_ap_config()` and `wifi_cfg_stop_http()` still link and
return `ESP_ERR_NOT_SUPPORTED`, so application code keeps compiling;
`cfg.enable_ap` is ignored.

## CLI

| Option | Default | Description |
|---|---|---|
| `CONFIG_WIFI_CFG_ENABLE_CLI` | n | Enable serial console CLI commands |

## Web UI

| Option | Default | Description |
|---|---|---|
| `CONFIG_WIFI_CFG_ENABLE_WEBUI` | n | Enable the embedded Web UI. **Requires `CONFIG_WIFI_CFG_ENABLE_SOFTAP`** — the Web UI is served by the portal's HTTP server |
| `CONFIG_WIFI_CFG_WEBUI_CUSTOM_PATH` | "" | Path to custom frontend files (LittleFS/SPIFFS). Requires `CONFIG_WIFI_CFG_ENABLE_WEBUI` |

## Network Provisioning (BLE)

Only two Kconfig symbols gate Network Provisioning — everything else
(security version, PoP, device name template, SRP6a username,
auto-reset behaviour, retry threshold) is plain runtime configuration
on `wifi_cfg_prov_config_t`. See [C API → `.prov_ble`](./c-api#prov-network-provisioning).

| Option | Default | Description |
|---|---|---|
| `CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING` | n | Enable ESP-IDF Wi-Fi/Network Provisioning manager |
| `CONFIG_WIFI_CFG_NETWORK_PROVISIONING_BLE` | y | Use the BLE scheme (currently the only transport supported by this library) |

The previous custom BLE GATT option (`WIFI_CFG_ENABLE_CUSTOM_BLE`) has
been **removed** in 0.1.0. See [MIGRATION.md][migrate] for upgrade
notes.

[migrate]: https://github.com/WiFiConfig/esp_wifi_config/blob/main/MIGRATION.md

## Improv WiFi

| Option | Default | Description |
|---|---|---|
| `CONFIG_WIFI_CFG_ENABLE_IMPROV_BLE` | n | Enable Improv BLE transport. Requires Bluetooth (`CONFIG_BT_ENABLED` with Bluedroid or NimBLE) and is mutually exclusive with `CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING` |
| `CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL` | n | Enable Improv Serial transport. **Mutually exclusive with `CONFIG_WIFI_CFG_ENABLE_CLI`** |
| `CONFIG_WIFI_CFG_IMPROV_SERIAL_UART_NUM` | 0 | UART port for Improv Serial. Requires `CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL` |
| `CONFIG_WIFI_CFG_IMPROV_SERIAL_BAUD` | 115200 | Baud rate for Improv Serial. Requires `CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL` |

Improv Serial and the CLI cannot coexist: both want to own the console
UART — the CLI prints human-readable output via `esp_console` while
Improv Serial frames binary bytes on the same stream. Enabling the CLI
makes `CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL` unsettable, and any
assignment to it is dropped without a warning.

## Common sdkconfig.defaults Combinations

### Basic WiFi (no extra features)

```kconfig
# No extra config needed — defaults work
```

### WiFi + Web UI

```kconfig
CONFIG_WIFI_CFG_ENABLE_WEBUI=y
```

### WiFi + Network Provisioning over BLE (NimBLE, recommended)

```kconfig
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=6144
CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING=y
CONFIG_WIFI_CFG_NETWORK_PROVISIONING_BLE=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```

Pair this sdkconfig with the runtime parameters (security version, PoP,
etc.) in your `wifi_cfg_init()` call — see the [C API
reference](./c-api#prov-network-provisioning).

### WiFi + Network Provisioning over BLE (Bluedroid)

```kconfig
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING=y
CONFIG_WIFI_CFG_NETWORK_PROVISIONING_BLE=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```

### WiFi + Improv BLE Only

```kconfig
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_WIFI_CFG_ENABLE_IMPROV_BLE=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```

### WiFi + Improv Serial + Network Provisioning BLE

```kconfig
# Improv Serial is independent of BLE and safe to combine with Network Provisioning.
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING=y
CONFIG_WIFI_CFG_NETWORK_PROVISIONING_BLE=y
CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```

### Improv Serial only, portal compiled out

The smallest useful build: provisioned over Improv Serial, with the
SoftAP portal, REST API and captive DNS left out of the image entirely.

```kconfig
CONFIG_WIFI_CFG_ENABLE_SOFTAP=n
CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL=y
CONFIG_WIFI_CFG_IMPROV_SERIAL_UART_NUM=0
```

### Everything that can coexist (WebUI + Network Provisioning + Improv Serial)

```kconfig
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_WIFI_CFG_ENABLE_WEBUI=y
CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING=y
CONFIG_WIFI_CFG_NETWORK_PROVISIONING_BLE=y
CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```

Note the absence of `CONFIG_WIFI_CFG_ENABLE_CLI`. Adding it would not
produce a build with both the CLI and Improv Serial — it would silently
drop `CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL` and give you a firmware with
no serial provisioning at all. To get the CLI instead, swap the two:

```kconfig
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_WIFI_CFG_ENABLE_CLI=y
CONFIG_WIFI_CFG_ENABLE_WEBUI=y
CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING=y
CONFIG_WIFI_CFG_NETWORK_PROVISIONING_BLE=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```
