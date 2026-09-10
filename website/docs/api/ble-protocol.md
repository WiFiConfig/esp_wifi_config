---
sidebar_position: 3
title: BLE Protocol Reference
description: Standard Espressif provisioning endpoints, all five library JSON extensions, and connection lifecycle
---

# BLE Protocol Reference

## What runs over BLE now

The BLE provisioning channel uses Espressif's `wifi_provisioning` manager on
IDF 5.x, with a compatibility shim for `network_provisioning` on IDF 6.x.
Standard clients use `proto-ver`, `prov-session`, `prov-scan`, `prov-config`,
and `prov-ctrl`. The library adds the five JSON endpoints documented below.

For packet formats, security handshakes, protobuf field numbers, and Python
recipes, see [BLE Wire Protocol](./ble-wire-protocol.md). For firmware setup,
see [BLE Provisioning](../provisioning/ble-gatt.md).

:::note Protocol migration
The custom JSON-over-GATT service (`0xFFE0`, characteristics `0xFFE1`–`0xFFE3`)
was removed in 0.1.0. These JSON extensions run inside an Espressif provisioning
session; they do not restore that old service. See the
[migration guide](https://github.com/WiFiConfig/esp_wifi_config/blob/main/MIGRATION.md).
:::

## Library-specific endpoints {#library-specific-protocomm-endpoints}

The backend creates these five characteristics before starting provisioning and
registers their handlers afterward. Discover them by their UTF-8 `0x2901`
descriptor names. Requests and responses pass through the negotiated session
cipher just like `prov-config`, but their plaintext is **UTF-8 JSON**.

Send `{}` to endpoints that ignore input. An empty request under Security 0 or 1
may produce no response. Serialize the entire encrypt/write/read/decrypt
transaction across the connection; see [transport rules](./ble-wire-protocol.md#transport).

| Endpoint | Request | Response |
|---|---|---|
| `esp-wifi-config-version` | `{}` | Library, IDF, application, and chip information |
| `esp-wifi-config-capabilities` | `{}` | Feature strings and store capacities |
| `esp-wifi-config-network-policy` | `{}` | Provisioning mode and retry/reconnect policy |
| `esp-wifi-config-network-info` | `{}` | Current station connection details |
| `esp-wifi-config-vars` | JSON operation | Read/write custom variables |

These schemas were checked against the
[backend at `d788922`](https://github.com/WiFiConfig/esp_wifi_config/blob/d788922/src/esp_wifi_config_prov_ble.c)
on September 10, 2026. Ignore unrecognized response fields for forward
compatibility. A missing endpoint is an availability error; do not send its
request to a guessed characteristic.

### Version

`esp-wifi-config-version` returns string fields `lib`, `idf`, `app`, `fw_version`,
`compile_time`, and `chip`. It includes `firmware_version` only when the application
supplies `prov_ble.firmware_version`. `fw_version` comes from the ESP-IDF application
descriptor; `lib` identifies the library release. These differ from the standard
`proto-ver` provisioning protocol version.

### Capabilities

`esp-wifi-config-capabilities` returns:

```json
{
  "capabilities": ["multi-network", "custom-vars", "softap"],
  "max_networks": 5,
  "max_vars": 10
}
```

This is an example; read the actual capacities from the device. The firmware
always lists `multi-network` and `custom-vars`. It adds `improv-serial`, `webui`,
and `cli` when their features are compiled in, and `softap` when the runtime
`enable_ap` flag is set. The list describes configuration, not proof that an
interface is currently active. It is separate from the standard `prov.cap` list.

### Network policy

`esp-wifi-config-network-policy` returns these fields when the library is initialized:

| Field | Type |
|---|---|
| `provisioning_mode` | String: `on_failure`, `when_unprovisioned`, `manual`, or `always` |
| `max_retry_per_network` | Number |
| `retry_interval_ms` | Number |
| `retry_max_interval_ms` | Number |
| `auto_reconnect` | Boolean |
| `max_reconnect_attempts` | Number |
| `saved_networks` | Number of stored networks |

The mode is a **string**, so the 0.2.0 C enum renumbering did not change this
endpoint's wire format. The field reports the configured mode; consult
[Provisioning Modes](../provisioning/modes.md) for backend restrictions on behavior.

### Network info {#network-info}

`esp-wifi-config-network-info` returns `{"connected": false}` while the station
is not in the library's Connected state, or if status cannot be obtained.
When connected, an example response is:

```json
{
  "connected": true,
  "ssid": "MyWiFi",
  "ip": "192.168.1.100",
  "netmask": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns": "192.168.1.1",
  "mac": "AA:BB:CC:DD:EE:FF",
  "bssid": "11:22:33:44:55:66",
  "hostname": "my-device",
  "rssi": -57,
  "quality": 86,
  "channel": 6,
  "uptime_ms": 4213
}
```

`mac` is the station MAC; `bssid` identifies the associated AP. RSSI is in dBm,
quality is a percentage, and `uptime_ms` is time since the station connected.
The fields come from `wifi_cfg_get_status()`.

Read this endpoint on the **existing BLE session**, immediately after confirmed
provisioning, if the client needs the IP address or other network details. Some
mobile SDK wrappers expose only a provisioning result, even though the standard
protobuf Connected response includes an IP address. Those clients can use this
endpoint while they still control the BLE connection.

IP assignment and delivery of the library's status event can lag the provisioning
success event. A short bounded retry of `connected:false` is appropriate. Keep
these reads inside the configured teardown/reboot window. Missing optional
network details must not undo an already confirmed provisioning success.

:::note Firmware history
The handler existed in 0.1.0, but the characteristic was not created, making the
endpoint unreachable. Commit
[`8176b3d`](https://github.com/WiFiConfig/esp_wifi_config/commit/8176b3d)
added the missing create call on August 12, 2026, included in 0.2.0.
The test harness records the fix as
[verified on hardware](https://github.com/WiFiConfig/test_harness/blob/main/rig/findings/library-prov-network-info-endpoint-is-never-created.md).
This supersedes the former standalone specification's pending-verification note.
:::

### Custom variables {#esp-wifi-config-vars-requestresponse-schema}

`esp-wifi-config-vars` accepts `op` with optional `key` and `value`. Omitted `op`
defaults to `list`. Keys and values are strings.

| Request | Successful response |
|---|---|
| `{"op":"list"}` | `{"vars":[{"k":"server_url","v":"https://example.com"}]}` |
| `{"op":"get","key":"server_url"}` | `{"key":"server_url","value":"https://example.com"}` |
| `{"op":"set","key":"server_url","value":"https://example.com"}` | `{"ok":true}` |
| `{"op":"del","key":"server_url"}` | `{"ok":true}` |

Errors include:

| Condition | Response |
|---|---|
| Missing key for `get` or `del` | `{"error":"missing_key"}` |
| Unknown key for `get` | `{"error":"not_found"}` |
| Unknown key for `del` | `{"ok":false,"error":"not_found"}` |
| Missing/non-string key or value for `set` | `{"error":"missing_key_or_value"}` |
| Store full or rejected value | `{"error":"store_full"}` or `{"error":"rejected"}` |
| Invalid JSON or unsupported operation | `{"error":"bad_json"}` or `{"error":"unknown_op"}` |

The handler also defines `empty_request`, but a zero-length GATT write may never
reach it. Use a nonempty JSON request. The current `get` handler reads values into
a 128-byte buffer; use the library's configured storage limits and validation
rules, documented under [Custom Variables](../guides/custom-variables.md).

## Reboot and connection handling {#connection-lifecycle}

Reboot after BLE provisioning is enabled by default. While the provisioning
manager is active, disconnecting after **credentials were received** can trigger
a reboot; that condition does not require a successful Wi-Fi connection.
The credentials-success event also starts a backstop timer, default **15 seconds**
(`prov_ble.reboot_max_wait_ms`).

Separately, `stop_provisioning_on_connect` and `provisioning_teardown_delay_ms`
can schedule interface shutdown after the station gets an IP, with the manager's
`cleanup_delay_ms` controlling its final shutdown grace. These paths can close
BLE before the reboot backstop. The 15-second timer is **not a guarantee that
endpoints remain available for 15 seconds**.

A client should:

1. Check the Set and Apply responses, then poll for confirmed Connected.
2. Fetch required JSON endpoint data before disconnecting.
3. If BLE drops before confirmation, treat the outcome as **unconfirmed** and
   check the device through an application-specific LAN/status mechanism.

A link drop or disappearance from advertising alone proves neither success nor
failure. Every new connection needs a new security handshake. If an encrypted
exchange fails or its completion is uncertain, discard the session's cipher
state and reconnect.

`prov_ble.disable_reboot_on_provisioning_success=true` disables the automatic
reboot paths; it does not disable the separate teardown-on-connect policy.
`prov_ble.stop_after_success` is ignored while reboot-on-success is enabled.
See [BLE configuration](../provisioning/ble-gatt.md#reboot-on-successful-provisioning)
for the firmware controls and reconnect workaround.

## Additional application endpoints {#what-was-intentionally-not-ported}

Supply `prov_ble.custom_endpoints` and `custom_endpoint_count` to register extra
named handlers. The library creates them before manager start and registers them
afterward. A handler receives plaintext after session decryption, owns its
serialization, and must allocate its response with `malloc` for protocomm to free.

The standard `prov-ctrl` Reset/Reprovision operations act on the provisioning
manager. They do not erase the library's multi-network or custom-variable stores.
Use the appropriate application/API operation for a factory reset; the former
custom GATT management commands were not retained as equivalent BLE endpoints.
