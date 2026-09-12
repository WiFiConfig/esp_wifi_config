---
sidebar_position: 3
title: Improv WiFi
description: Open-standard provisioning via Web Bluetooth and Web Serial
---

# Improv WiFi

[Improv WiFi](https://www.improv-wifi.com/) is an open standard by ESPHome for provisioning IoT devices over BLE or Serial using web browsers and companion apps.

:::note Mutually exclusive with Network Provisioning BLE
`CONFIG_WIFI_CFG_ENABLE_IMPROV_BLE` and
`CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING` cannot both be enabled in
a single firmware build — they each want to own the BLE GAP advertising
and the NimBLE/Bluedroid host. Pick the protocol that matches your
provisioning client tooling, or ship two firmware variants.

Improv **Serial** (`CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL`) is independent
of BLE and remains safe to enable alongside Network Provisioning BLE.
:::

:::warning ESP-IDF: Improv Serial and the CLI cannot coexist
`CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL` depends on
`CONFIG_WIFI_CFG_ENABLE_CLI` being off. Both want to own the console
UART — the CLI prints human-readable output via `esp_console` while
Improv Serial frames binary bytes on the same stream.

If the CLI is enabled, kconfig drops any assignment to
`CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL` **without a warning**: the build
succeeds and ships with no serial provisioning. Check the generated
`sdkconfig` for the symbol rather than assuming your fragment applied.
:::

## Enabling Improv

### Arduino

For native Arduino Improv BLE, put `-DWIFI_CFG_ARDUINO_IMPROV_BLE=1` in the
sketch's `build_opt.h`, or in PlatformIO's `build_flags`. The board package's
stock BLE host is the default. For **NimBLE-Arduino 2.5.1**, install that optional
library and also set `-DWIFI_CFG_ARDUINO_NIMBLE=1`; the `NimBLEImprov` example
includes both flags. See the [Arduino guide](../arduino.md#nimble-arduino) for
setup, RAM measurements and BLE ownership limits.

Improv Serial uses `-DWIFI_CFG_ARDUINO_IMPROV_SERIAL=1` and an
application-supplied `Stream`; see [Arduino serial setup](../arduino.md#ble-and-serial-details).
The two BLE protocols remain mutually exclusive, while Serial can accompany
either one. Arduino builds do not use the Kconfig settings below.

### ESP-IDF Kconfig

```kconfig
# BLE transport (requires Bluetooth enabled)
CONFIG_WIFI_CFG_ENABLE_IMPROV_BLE=y

# Serial transport (optional)
CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL=y
CONFIG_WIFI_CFG_IMPROV_SERIAL_UART_NUM=0
CONFIG_WIFI_CFG_IMPROV_SERIAL_BAUD=115200
```

### Runtime Config

```c
wifi_cfg_init(&(wifi_cfg_config_t){
    WIFI_CFG_DEFAULTS,
    // provisioning_mode is already WIFI_PROV_ON_FAILURE from the macro.
    .stop_provisioning_on_connect = true,
    .enable_ap = true,
    // Transports selected via Kconfig (CONFIG_WIFI_CFG_ENABLE_IMPROV_BLE / _SERIAL)
    .improv = {
        .firmware_name = "my_project",
        .firmware_version = "1.0.0",
        .device_name = "My Device",
        .on_identify = my_identify_callback,  // Optional: flash LED/make noise on Identify
    },
});
```

## How to Provision

### Via Web Bluetooth (Chrome/Edge)

1. Open [improv-wifi.com](https://www.improv-wifi.com/) in Chrome or Edge
2. Click "Connect device via Bluetooth"
3. Select the device from the browser pairing dialog
4. Enter WiFi credentials — the device connects and returns its IP

### Via ESPHome Companion App

1. Install the ESPHome app (Android/iOS)
2. The device appears automatically for Improv provisioning
3. Tap and enter WiFi credentials

### Via Web Serial (if enabled)

1. Open [improv-wifi.com](https://www.improv-wifi.com/) in Chrome or Edge
2. Click "Connect device via Serial"
3. Select the serial port and enter WiFi credentials

## Supported RPC Commands

| Command | ID | Description |
|---|---|---|
| Send WiFi Settings | 0x01 | Provide SSID + password, device connects |
| Identify | 0x02 | Flash LED / beep (calls `on_identify` callback) |
| Get Device Info | 0x03 | Returns firmware name, version, chip, device name |
| Get WiFi Networks | 0x04 | Triggers a WiFi scan and returns results |

### How many networks a scan returns

The two Improv specifications shape this command differently, and the library
follows each transport's own.

**Serial** sends one response per network and then an empty one to mark the
end, so the list is not bounded by anything the protocol imposes.

**BLE** sends a single response holding every network, which the format caps: a
result's length field is one byte, so no response can carry more than 255 bytes
of payload — roughly eleven networks at three strings each. The library asks
for an ATT MTU of 517, so on a typical link that 255-byte ceiling is what you
hit; a client that negotiates a smaller MTU gets a shorter list still, because
the response is also kept inside what one notification can carry. Whichever
bound bites first, the list is cut to fit.

What survives the cut is chosen for you: scan results arrive strongest-first
and are already deduplicated by SSID, so the networks that drop off are the
faintest ones. A device in a crowded band will not show a user every SSID over
BLE, and cannot — but it will show them the nearby ones.

## BLE Stack Requirements

In ESP-IDF, Improv BLE requires `CONFIG_BT_ENABLED=y` and a NimBLE or Bluedroid host
stack. The BLE stack is initialised automatically when Improv BLE is
enabled — the library does not need any other Kconfig opt-in. See the
[with_improv example](https://github.com/WiFiConfig/esp_wifi_config/tree/main/examples/with_improv)
for a complete sdkconfig.

If you need the official ESP-IDF provisioning protocol instead of
Improv (e.g. for use with the Espressif "ESP BLE Provisioning" mobile
apps), see [BLE Provisioning](./ble-gatt.md). The two are mutually
exclusive at compile time.
