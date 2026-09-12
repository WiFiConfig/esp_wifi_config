---
sidebar_position: 3
title: Arduino
---

# Native Arduino support

Use this library directly in an Arduino sketch with the **Espressif ESP32 board
package 3.3.11**. It builds with Arduino IDE/CLI and PlatformIO's native Arduino
framework. No ESP-IDF project, menuconfig, or rebuilt board package is required.
The original ESP-IDF C API remains available alongside `ESPWiFiConfig.h`.

Native Arduino and optional NimBLE-Arduino support are included in **0.3.1**.
Install the [0.3.1 library ZIP](https://github.com/WiFiConfig/esp_wifi_config/archive/refs/tags/0.3.1.zip)
or use the PlatformIO dependency below.

## Install and connect

1. Install **esp32 by Espressif Systems**, version 3.3.11, in Boards Manager.
2. Download the [0.3.1 library ZIP](https://github.com/WiFiConfig/esp_wifi_config/archive/refs/tags/0.3.1.zip)
   and install it using **Sketch → Include Library → Add .ZIP Library**.
3. Open **ESP WiFi Config → arduino → Basic** and select your board.
4. For the full BLE and web UI examples, select **Partition Scheme → Minimal
   SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**, or another layout with sufficient
   application space. The stock BLE examples exceed the original ESP32's default
   1.25 MiB application slot.
5. Upload. When no saved network is available, join `ESP32-Config` and browse to
   `http://192.168.4.1`. The embedded interface needs no filesystem upload.

```cpp
#include <ESPWiFiConfig.h>

ESPWiFiConfig wifiConfig;

void setup() {
  Serial.begin(115200);
  esp_err_t result = wifiConfig.begin();
  if (result != ESP_OK) Serial.println(esp_err_to_name(result));
}

void loop() {
  // The library's background tasks manage connection and provisioning.
  delay(1000);
}
```

For custom settings, use `auto config = ESPWiFiConfig::defaults()`, set fields,
then call `wifiConfig.begin(config)`. The argumentless `begin()` enables the
portal; `defaults()` preserves the C API defaults, including `enable_ap=false`.
Examples cover multiple networks, custom variables, HTTP clients, and lifecycle.
The configuration's pointer fields must remain valid until `end()` completes.

## Build options

Runtime behavior uses the same `wifi_cfg_config_t` as IDF. Features that change
which sources are compiled use the following Arduino-specific options:

| Option | Default | Meaning |
| --- | --- | --- |
| `WIFI_CFG_ARDUINO_SOFTAP` | `1` | Captive portal, DNS and HTTP API |
| `WIFI_CFG_ARDUINO_WEBUI` | Same as SoftAP | Embedded frontend |
| `WIFI_CFG_ARDUINO_PROV_BLE` | `0` | Espressif provisioning with the existing custom endpoints |
| `WIFI_CFG_ARDUINO_IMPROV_BLE` | `0` | Improv BLE, alternative to Espressif provisioning |
| `WIFI_CFG_ARDUINO_IMPROV_SERIAL` | `0` | Improv using an application-supplied `Stream` |
| `WIFI_CFG_ARDUINO_NIMBLE` | `0` | Use the optional NimBLE-Arduino host for the selected BLE protocol |

For Arduino IDE/CLI, put build flags in a file named `build_opt.h` next to the
sketch. Despite its extension, this is a compiler-options file, not a C header:

```text
-DWIFI_CFG_ARDUINO_PROV_BLE=1
```

The BLE, Improv and NoPortal examples include this file. For PlatformIO, use
`build_flags` with the same definitions. Defining an option only inside the
`.ino` file does **not** configure the separately compiled library sources.
Both BLE protocols cannot be enabled together. Disabling SoftAP also disables
the web UI by default. Library capacity limits such as
`CONFIG_WIFI_CFG_MAX_NETWORKS` can be supplied as build flags too.

The board package's SDK configuration is fixed. Do not redefine `CONFIG_BT_*`
or protocomm security options to manufacture a feature absent from its binaries.
By default, the library uses the host supplied by the selected stock package.
The opt-in below selects a separately compiled host instead. ESP32-S2 has no BLE; chips without native Wi-Fi are rejected.

## NimBLE-Arduino

For classic ESP32, this option saved about 51–58 KiB of RAM while advertising
in our tests. On S3, keep the stock SDK NimBLE host for resource efficiency.
See [the measured comparison](#nimble-arduino-measurements). The stock host
remains the default on every board until you set `WIFI_CFG_ARDUINO_NIMBLE=1`.

Install **NimBLE-Arduino by h2zero, version 2.5.1**, from Library Manager (or
`arduino-cli lib install NimBLE-Arduino@2.5.1`). Open `NimBLEProvisioning` or
`NimBLEImprov`, or add the following to your sketch's `build_opt.h`:

```text
-DWIFI_CFG_ARDUINO_PROV_BLE=1
-DWIFI_CFG_ARDUINO_NIMBLE=1
```

For Improv, replace the first flag with `-DWIFI_CFG_ARDUINO_IMPROV_BLE=1`.
For PlatformIO, add `h2zero/NimBLE-Arduino@2.5.1` to `lib_deps` and the same flags
to `build_flags`. The dependency is optional: ordinary sketches need not install it.

This changes the BLE host and GATT transport. Espressif's provisioning manager,
Security 0/1/2, proof of possession, endpoint names/UUIDs and wire payloads stay
the same. Improv retains its existing framing, checksum, RPC and notification
behavior. Wi-Fi, SoftAP and the portal retain their usual build/runtime settings.

The adapter owns the BLE stack and accepts one provisioning client at a time.
Start it before other BLE code. The transport refuses an already initialized
controller or `NimBLEDevice` and logs `ESP_ERR_INVALID_STATE`; Wi-Fi initialization
can still succeed when a provisioning interface fails. It does not share an application's
existing GATT server, replace its callbacks, or offer Bluetooth Classic.
Stopping secure provisioning releases NimBLE host allocations; Improv releases
them on `end()`. The controller's BLE reservation is retained for restart,
overriding `prov_ble.memory_policy` with a log message; NimBLE-Arduino itself
releases Classic BT memory on ESP32.
`keep_ble_on_after_stop` remains unsupported. Custom service UUIDs are supported;
manufacturer data and the advertised name must fit the 31-byte scan response.

The stock ESP32 package uses Bluedroid; the stock S3 package already uses
Espressif's NimBLE. That bundled host is distinct from **NimBLE-Arduino**.
Resource results therefore need comparisons on the same chip, firmware and
application configuration. See the validation section for measured results.

## Arduino coexistence

Arduino initializes and owns its network interfaces. The library adopts them
and disconnects any existing association before choosing a saved network. It
uses Arduino for Wi-Fi mode changes and shutdown, preserving Arduino status
and client APIs. Use `WiFi.status()`, `WiFi.localIP()`, `NetworkClient`, and
`HTTPClient` normally after connecting. The Client example demonstrates this.

While the manager runs, use its connect/disconnect/scan APIs rather than driving
the radio simultaneously with `WiFi.begin()`, `WiFi.scanNetworks()`, or another
connection manager. Arduino auto-reconnect is disabled and restored on `end()`.
The stock core also performs one internal retry on its first failed association;
radio-level attempt counts can therefore differ from the library retry count.

`end(false)` leaves the Arduino radio available to the application;
`end(true)` switches it off through Arduino. Stop using the manager from other
tasks before calling `end()`. Event callbacks registered with `onEvent()` run
on the ESP event task: do not block or call `end()` there. Remove registered
callbacks with `removeEvent()` before their context is destroyed.

Network and variable records retain their existing `wifi_cfg` NVS format. The
library does not erase the shared Arduino NVS partition to recover initialization
errors. Its factory-reset API still resets the library's own configuration.

The HTTP server is `esp_http_server`. Existing shared-httpd integration remains
available through the C API. Arduino `WebServer` and AsyncWebServer are distinct
servers; they cannot share this handle or bind the same port. Other BLE libraries
must not register conflicting global callbacks or concurrently own provisioning.

## BLE and serial details

The stock 3.3.11 package supports provisioning Security 0, 1 and 2. Security 1
remains the default; configure a product-specific proof of possession. Security 2
requires the same precomputed salt/verifier as IDF. Wire endpoints and payloads
are unchanged; existing provisioning clients continue to use the same protocol.

The bundled managed `network_provisioning` API lacks `random_addr`,
`keep_ble_on_after_stop`, and `wifi_conn_attempts`. Non-default selections return
`ESP_ERR_NOT_SUPPORTED` at initialization. A security scheme missing from a
different SDK build is also rejected. The existing IDF backend retains its own
controls. With the stock host, BLE memory is reserved before Arduino startup
and remains subject to the configured provisioning cleanup policy. The optional
NimBLE-Arduino host instead follows the restartable cleanup behavior described
[above](#nimble-arduino).

For Improv Serial, initialize a `HardwareSerial` or USB serial stream and call
`wifiConfig.setImprovSerial(stream)` before `begin()`. Only Improv may read the
stream while active. Route application and SDK logs elsewhere; the supplied
UART0 example disables SDK logging. USB CDC is supported when enabled in the
board's Tools menu. Baud and pin selection belong to the stream's `begin()`;
the IDF UART configuration fields do not configure an Arduino stream.
The `esp_console` CLI remains an IDF feature.

## PlatformIO

Use the pinned pioarduino platform, which supplies Arduino 3.3.11:

```ini
[env:esp32s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_build.partitions = min_spiffs.csv
lib_deps = thorrak/esp_wifi_config@0.3.1
```

The repository's `examples/arduino/PlatformIO` project instead links the local
checkout, so it also builds uncommitted library changes. The official PlatformIO
Espressif32 platform currently supplies an older Arduino core and is not the
tested installation path.

## Validation and maintenance

CI compiles native sketches for ESP32, S3, C3, C6 and Wi-Fi-only S2, alongside the
existing IDF example matrix. Physical Raspberry Pi 4 harness runs on 2026-09-11
validated native 3.3.11 builds on classic ESP32 and ESP32-S3: saved-network boot,
portal/REST behavior, reconnect/failover and provisioning lifecycle. S3 also
passed managed BLE provisioning at Security 0/1/2, ten secure BLE reconnects,
Improv BLE, native USB CDC Improv and ten Arduino status/client/begin/end cycles. Classic
ESP32 passed managed Security 1 provisioning, Improv BLE/UART, and ten Improv
BLE reconnects. Other targets currently have compile coverage only.

The existing reconnect limitation remains: an AP returning during a backoff
wait is not noticed promptly. The bench cannot certify brownouts, power-loss
recovery or controlled weak-signal behavior. The ten-cycle resource check is
not a long-duration soak test. Classic ESP32 applications without PSRAM need
particular attention to internal RAM when enabling Bluedroid; the normal BLE
example passed, but the instrumented HIL build required a smaller telemetry
queue. Build and evidence details are in the harness's Arduino validation guide.

### NimBLE-Arduino measurements

The same Pi bench validated **56 distinct NimBLE-Arduino cases** on ESP32 and
S3, covering Security 0/1/2, custom endpoints, Improv notifications and malformed
commands, portal coexistence, and ten reconnects for each BLE protocol on each
board. Separate ESP32 probes exercised three complete begin/provision/end cycles
per protocol and backend, with firmware identity and DHCP verification.

With identical portal-enabled ESP32 sketches, core 3.3.11 and NimBLE-Arduino
2.5.1, the optional host provided:

| ESP32 workload | Additional free heap while advertising | Application image reduction |
| --- | ---: | ---: |
| Security 1 provisioning | About 58 KiB | 429 KiB (25%) |
| Improv BLE | About 51 KiB | 434 KiB (27%) |

This is a useful increase in RAM headroom on the original ESP32. Both protocols
ran with the normal 32-record HIL telemetry queue. The comparison sketches
exclude that instrumentation, so these savings do not depend on reducing the
queue. The timing samples did not establish a provisioning-speed improvement.

On **S3**, the stock package already uses SDK NimBLE. Matched HIL flows showed
NimBLE-Arduino leaving roughly 1–3 KiB less free heap, with application size
changing by only a few KiB. Keep the stock host there for resource efficiency.
C3/C6 have compile coverage only. These short runs do not establish long-term
memory stability or weak-signal performance. Exact measurements, firmware
identities and corrected failures are recorded in the harness's
`docs/nimble-arduino-validation-2026-09-11.md` report.

Frontend changes require `python3 tools/generate_arduino_assets.py` after rebuilding
the frontend. CI verifies that these checked-in arrays exactly match
`frontend/dist`; users installing the library need no Python or Node tools.
