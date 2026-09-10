---
sidebar_position: 3.1
title: BLE Wire Protocol
description: Transport, protobuf fields, security handshakes, and Python implementation notes for Espressif BLE provisioning clients
---

# BLE Wire Protocol

This reference is for authors of provisioning clients. For firmware configuration,
start with [BLE Provisioning](../provisioning/ble-gatt.md). For the library's JSON
extensions, see [BLE Protocol Reference](./ble-protocol.md).

It consolidates the former `bluetooth_spec.md` research notes. The wire details
below were checked against **ESP-IDF v5.4.3** source, and the library-specific
behavior against `esp_wifi_config` at **`d788922`**, on September 10, 2026.
This was a source review, not a new hardware test run. The earlier notes recorded
Security 0/1/2 provisioning on ESP32 with IDF 5.4.3; later hardware findings are
identified separately in the [library endpoint reference](./ble-protocol.md#network-info).

The original notes mixed Android SDK implementation choices with firmware rules,
then corrected some of those rules in later appendices. This page presents the
reconciled Wi-Fi protocol. Android scan timeouts and polling intervals are client
choices, not protocol requirements.

## Scope and sources

The firmware delegates the standard endpoints to Espressif's provisioning manager.
Its on-wire messages are protobuf over BLE GATT, with a session security layer.
The library's additional endpoints use JSON inside that same session.

The source baseline for this reference is:

- [ESP-IDF v5.4.3 Wi-Fi protobuf definitions](https://github.com/espressif/esp-idf/tree/v5.4.3/components/wifi_provisioning/proto).
- [ESP-IDF v5.4.3 session protobuf definitions](https://github.com/espressif/esp-idf/tree/v5.4.3/components/protocomm/proto).
- [Bluedroid transport](https://github.com/espressif/esp-idf/blob/v5.4.3/components/protocomm/src/transports/protocomm_ble.c)
  and [NimBLE transport](https://github.com/espressif/esp-idf/blob/v5.4.3/components/protocomm/src/transports/protocomm_nimble.c).
- [Security 1](https://github.com/espressif/esp-idf/blob/v5.4.3/components/protocomm/src/security/security1.c),
  [Security 2](https://github.com/espressif/esp-idf/blob/v5.4.3/components/protocomm/src/security/security2.c),
  and [SRP6a implementation](https://github.com/espressif/esp-idf/blob/v5.4.3/components/protocomm/src/crypto/srp6a/esp_srp.c).
- This repository's [BLE backend](https://github.com/WiFiConfig/esp_wifi_config/blob/d788922/src/esp_wifi_config_prov_ble.c)
  and [vendored Python protocol modules](https://github.com/WiFiConfig/esp_wifi_config/tree/d788922/tools/wifi_ble_cli/esp_prov).

IDF 5.x uses `wifi_provisioning`, whose generated message names appear below.
The newer `network_provisioning` component and some mobile SDKs use names such as
`NetworkConfigPayload` and `CmdSetWifiConfig`. Protobuf serializes field numbers
and enum values, not message names; the corresponding Wi-Fi operations interoperate.
Generate bindings from the component version used by the target firmware.

**Thread and cloud association are outside this library's documented provisioning
surface.** Their presence in an SDK's protobuf files does not make them available
on an `esp_wifi_config` device. The former notes' Thread and RainMaker schemas have
therefore not been carried into this Wi-Fi reference.

## BLE transport {#transport}

### Discover endpoints by name

Obtain the provisioning service UUID from the device advertisement, QR data, or
product configuration. Enumerate its characteristics and read each **Characteristic
User Description** descriptor (`0x2901`, full UUID
`00002901-0000-1000-8000-00805f9b34fb`). Its UTF-8 value is the endpoint name.
Build a map from that name to the characteristic.

The default IDF v5.4.3 service UUID is
`1775244d-6b43-439b-877c-060f2d9bed07`. Applications may override it with
`prov_ble.service_uuid128`. The default mapping below is useful for inspecting a
trace; descriptor discovery is the client contract.

| Endpoint | ID | Default characteristic UUID |
|---|---|---|
| `prov-ctrl` | `0xFF4F` | `1775ff4f-6b43-439b-877c-060f2d9bed07` |
| `prov-scan` | `0xFF50` | `1775ff50-6b43-439b-877c-060f2d9bed07` |
| `prov-session` | `0xFF51` | `1775ff51-6b43-439b-877c-060f2d9bed07` |
| `prov-config` | `0xFF52` | `1775ff52-6b43-439b-877c-060f2d9bed07` |
| `proto-ver` | `0xFF53` | `1775ff53-6b43-439b-877c-060f2d9bed07` |

Custom endpoint IDs start at `0xFF54`; their allocation depends on registration
order. Do not substitute a guessed characteristic when a required name is absent.

### Write, then read

For each transaction:

1. Write the complete request to the endpoint characteristic **with response**.
2. Wait for the write acknowledgement.
3. Read the same characteristic to obtain the response.

The acknowledgement confirms the GATT write, not successful provisioning. Inspect
the returned protocol status. The reference transports return replies through
reads; clients must not wait for a notification instead.

Serialize complete transactions across **all endpoints in a session**, including
encryption before the write and decryption after the read. Two concurrent requests
can overwrite a response or desynchronize the shared cipher state.

### MTU and long operations

Request a large MTU where the platform exposes that option; the original Android
client requested 512. Security 2 exchanges public keys of about 384 bytes plus
protobuf framing. A normal ATT write carries at most `MTU - 3` bytes; an initial
read response carries at most `MTU - 1` bytes.

There is **no application-level chunk envelope**. A large logical GATT operation
may nevertheless span ATT packets: the v5.4.3 Bluedroid transport implements
Prepare/Execute Write and Read Blob. Verify that the selected client backend
supports the required long writes and reads at its negotiated MTU. Do not split
one encrypted protobuf into independent ordinary writes.

Bluedroid's characteristic/prepare-write limit is **481 bytes** when Security 2
support is compiled in and **257 bytes** otherwise. These are limits of that
transport/version, not universal NimBLE limits or guarantees for every custom
response. Include the Security 2 authentication tag when budgeting payload size.

### Nonempty requests

Send `b"ESP"` to `proto-ver`. For the library's read-only JSON endpoints, send
`b"{}"`. A zero-length request under Security 0 or 1 can become a zero-length GATT
write that produces no response. Security 2 adds a tag even to empty plaintext,
but using `{}` consistently avoids transport-dependent behavior.

## Version and security selection {#discovery}

Read `proto-ver` **before** establishing the security session. Both its request
and response are plaintext. The firmware ignores the request contents; `ESP` is
the conventional nonempty request.

An example response is:

```json
{
  "prov": {
    "ver": "v1.1",
    "sec_ver": 2,
    "sec_patch_ver": 1,
    "cap": ["wifi_scan"]
  }
}
```

| Field | Client interpretation |
|---|---|
| `prov.ver` | Provisioning protocol version, not the `esp_wifi_config` release number. |
| `prov.sec_ver` | `0`, `1`, or `2`; select the matching handshake. Reject unsupported values. |
| `prov.sec_patch_ver` | Security implementation patch; this reference supports Security 2 patch **1**. |
| `prov.cap` | Capability strings. Tolerate unknown entries. IDF v5.4.3 advertises `wifi_scan`, with `no_sec` or `no_pop` when applicable. |

`no_sec` indicates Security 0. For Security 1, `no_pop` means skip PoP mixing.
Do not infer from `no_pop` that Security 2 needs no username/password: its client
still authenticates against the device's salt and verifier. The configured
`sec_ver` controls the handshake.

Application metadata may add other top-level objects through `prov_ble.app_info`.
The library's feature flags are returned separately by
`esp-wifi-config-capabilities`; they are not the `prov.cap` list.

Never silently downgrade security because discovery failed or returned an unknown
version. The bundled CLI selects the scheme explicitly with `--sec-ver`; for
Security 2 it defaults a *missing* patch field to 1 for compatibility and rejects
other patch versions. A new client can instead require an explicit patch 1.
Neither policy should enable the historical reused-nonce mode.

## Protobuf framing {#protobuf}

Standard application messages are raw proto3 bytes, encrypted when the selected
scheme requires it. There is no extra application length prefix. `proto-ver`
uses JSON; custom endpoints choose their own serialization.

Generate bindings from these source files rather than transcribing the tables:

```text
components/protocomm/proto/
  constants.proto  session.proto  sec0.proto  sec1.proto  sec2.proto
components/wifi_provisioning/proto/
  wifi_constants.proto  wifi_config.proto  wifi_scan.proto  wifi_ctrl.proto
```

Each root payload contains a message-type discriminator and a command/response
`oneof`. Set both consistently. Proto3 omits default-valued scalar fields from
the wire. An empty command still needs its enclosing `oneof` field to be present.
Preserve unknown fields/enums where the binding supports it, and validate the
expected response branch before acting on default values.

The common `Status` enum is:

| Value | Name |
|---|---|
| 0 | `Success` |
| 1 | `InvalidSecScheme` |
| 2 | `InvalidProto` |
| 3 | `TooManySessions` |
| 4 | `InvalidArgument` |
| 5 | `InternalError` |
| 6 | `CryptoError` |
| 7 | `InvalidSession` |

### Session envelope

`SessionData.sec_ver` is field **2**. Its `oneof proto` uses `sec0=10`,
`sec1=11`, and `sec2=12`. Wire security values are **0/1/2**; do not serialize
the library's C configuration enum, which also has a `DEFAULT` member.

The handshake is sent to `prov-session` without wrapping the whole message in the
application cipher. Some handshake fields themselves contain encrypted proofs.
Check the scheme, expected message type/branch, status, lengths, and proof on
every response.

| Scheme | Payload `msg` field | Command/response values | `oneof` field numbers |
|---|---|---|---|
| Security 0 | `Sec0Payload.msg = 1` | command 0, response 1 | `sc=20`, `sr=21` |
| Security 1 | `Sec1Payload.msg = 1` | command0 0, response0 1, command1 2, response1 3 | `sc0=20`, `sr0=21`, `sc1=22`, `sr1=23` |
| Security 2 | `Sec2Payload.msg = 1` | command0 0, response0 1, command1 2, response1 3 | `sc0=20`, `sr0=21`, `sc1=22`, `sr1=23` |

| Message | Fields (`name = number`) |
|---|---|
| `S0SessionCmd` | empty |
| `S0SessionResp` | `status=1` |
| `SessionCmd0` | `client_pubkey=1` |
| `SessionResp0` | `status=1`, `device_pubkey=2`, `device_random=3` |
| `SessionCmd1` | `client_verify_data=2` — deliberately no field 1 |
| `SessionResp1` | `status=1`, `device_verify_data=3` |
| `S2SessionCmd0` | `client_username=1`, `client_pubkey=2` |
| `S2SessionResp0` | `status=1`, `device_pubkey=2`, `device_salt=3` |
| `S2SessionCmd1` | `client_proof=1` |
| `S2SessionResp1` | `status=1`, `device_proof=2`, `device_nonce=3` |

Keys, random values, proofs, nonce, and username are `bytes` fields. Encode the
username as UTF-8.

## Security 0

Send a `SessionData` containing `sec_ver=0` and `sec0.sc=S0SessionCmd{}`.
Validate the response `sec0.sr.status`. This is a one-round handshake even though
it establishes no encryption: application `encrypt` and `decrypt` are identity
functions. JSON extension payloads remain JSON, not protobuf.

## Security 1: X25519 and AES-CTR {#security-1}

Security 1 has two handshake rounds:

1. Generate an ephemeral X25519 key pair and send the 32-byte public key in
   `SessionCmd0.client_pubkey`.
2. Receive `SessionResp0` with a 32-byte device public key and a 16-byte
   `device_random`. Compute the X25519 shared secret. If PoP is nonempty, XOR
   that 32-byte secret with `SHA256(UTF8(PoP))`.
3. Initialize AES-256-CTR with that key and `device_random` as its IV. Encrypt
   the device public key to form `SessionCmd1.client_verify_data`.
4. Decrypt `SessionResp1.device_verify_data` using the **same stream** and verify
   that it equals the client public key. Only then is the session established.

One continuous CTR stream serves both directions. Its first 32 bytes encrypt
the device key; its next 32 decrypt the device's verification token. Every
application request and response continues from there. Preserve unused bytes
within a cipher block across messages: do **not** round each packet up to a
16-byte boundary, initialize separate directional ciphers, or reset the IV.

### Python cipher recipe

The following helper covers the cipher part of the handshake. The caller must
validate the protobuf envelope and status before passing response fields to it.

```python
import hashlib
import hmac
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PublicKey
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes


def security1_cipher(private_key, device_public_key, device_random, pop=""):
    if len(device_public_key) != 32 or len(device_random) != 16:
        raise ValueError("Invalid Security 1 key or IV length")
    shared = private_key.exchange(X25519PublicKey.from_public_bytes(device_public_key))
    if pop:
        digest = hashlib.sha256(pop.encode("utf-8")).digest()
        shared = bytes(a ^ b for a, b in zip(shared, digest))
    stream = Cipher(algorithms.AES(shared), modes.CTR(device_random)).encryptor()
    client_verify = stream.update(device_public_key)
    return stream, client_verify


def verify_security1_device(stream, device_verify, client_public_key):
    if len(device_verify) != 32:
        raise ValueError("Invalid Security 1 proof length")
    if not hmac.compare_digest(stream.update(device_verify), client_public_key):
        raise ValueError("Security 1 device proof failed")
    # Keep this same stream: stream.update() encrypts AND decrypts application data.
```

## Security 2: SRP6a and AES-GCM {#security-2}

The device must be configured with a precomputed salt and verifier derived from
the client's username and password. A raw PoP string alone is insufficient on
the firmware side. See [Security versions](../provisioning/ble-gatt.md#security-versions)
for setup.

Use the Espressif-compatible SRP implementation when possible. The proof encoding
below matters when porting to a new language; a generic SRP package may use
different padding or proof construction.

### SRP parameters and byte encodings {#srp-encoding}

| Parameter | Value |
|---|---|
| Group | RFC 5054 3072-bit `N`, generator `g=5` |
| Hash `H` | SHA-512, producing **64 bytes** |
| `I`, `P` | Username and password encoded as UTF-8 |
| `PAD(n)` | Unsigned big-endian integer, left-padded to **384 bytes** |
| `INT(n)` | Minimal unsigned big-endian encoding of a positive integer |
| `s` | The **received salt bytes**, retaining any leading zero bytes |
| `A_wire`, `B_wire` | Exact public-key bytes sent/received; the reference emits minimal unsigned big-endian integers |
| `K`, `M1`, `M2` | Raw **64-byte hash outputs**; do not strip leading zeros |

The former notes described every SRP value as a minimal integer. That is not a
safe rule for salt, hashes, or proofs. The firmware hashes the salt and public
key buffers with their received lengths and compares the full 64-byte proof.

For a fresh random client secret `a`, calculate `A = g^a mod N`. Send the username
and `A_wire = INT(A)` in `S2SessionCmd0`. After receiving the salt and device public
key `B_wire`, the client calculations are:

```text
k  = integer(H(PAD(N) || PAD(g)))
u  = integer(H(PAD(A) || PAD(B)))
x  = integer(H(s || H(I || ":" || P)))
S  = (B - k * g^x)^(a + u*x) mod N
K  = H(INT(S))

c  = H(PAD(N)) XOR H(PAD(g))
M1 = H(c || H(I) || s || A_wire || B_wire || K)
M2 = H(A_wire || M1 || K)
```

`||` concatenates bytes; `integer()` interprets a hash as an unsigned big-endian
integer. Reject `B mod N == 0` and `u == 0` before deriving a session.
Send `M1` in `S2SessionCmd1.client_proof`. Verify the device's `M2` with a
constant-time comparison before using the first **32 bytes of `K`** as the
AES-256 key. Also validate the response status and nonce length.

### Patched GCM nonce

The v5.4.3 firmware implements Security 2 patch **1**. Its `device_nonce` is
exactly **12 bytes**:

```text
session_id[8] || big-endian uint32(counter)
```

Seed the counter from the last four received bytes; this firmware initializes it
to **1**. Each endpoint message uses the fixed eight-byte prefix plus the current
counter, then advances the counter once. Each peer maintains **one counter for
both encryption and decryption**:

```text
client encrypts request at 1  -> device decrypts at 1
device encrypts response at 2 -> client decrypts at 2
client encrypts request at 3  -> device decrypts at 3
```

AES-GCM is initialized for each message. The wire value is ciphertext followed
by a **16-byte authentication tag**, with **no additional authenticated data**.
Do not use a static 16-byte nonce, fall back to that mode when the patch is
missing, or wrap the counter and reuse a nonce. Establish a new session before
counter exhaustion.

### Python GCM recipe

Construct this object only after validating the SRP server proof. A failed
authentication tag or uncertain transport exchange invalidates the session;
discard the object and reconnect rather than retrying on its old counter.

```python
from cryptography.hazmat.primitives.ciphers.aead import AESGCM


class Security2Cipher:
    def __init__(self, session_key, device_nonce):
        if len(session_key) != 64 or len(device_nonce) != 12:
            raise ValueError("Invalid Security 2 key or nonce length")
        self.aes = AESGCM(session_key[:32])
        self.prefix = device_nonce[:8]
        self.counter = int.from_bytes(device_nonce[8:], "big")

    def _next_nonce(self):
        if self.counter >= 0xFFFFFFFF:
            raise ValueError("Establish a new session before nonce exhaustion")
        nonce = self.prefix + self.counter.to_bytes(4, "big")
        self.counter += 1
        return nonce

    def encrypt(self, plaintext):
        return self.aes.encrypt(self._next_nonce(), plaintext, None)

    def decrypt(self, ciphertext):
        return self.aes.decrypt(self._next_nonce(), ciphertext, None)
```

## Wi-Fi application messages {#wifi-messages}

After the handshake, each transaction is:

```text
serialize -> encrypt -> GATT write/read -> decrypt -> parse and validate response
```

Use identity encryption for Security 0. `proto-ver` and `prov-session` remain
outside this application wrapper. The JSON custom endpoints use the wrapper,
with UTF-8 JSON in place of serialized protobuf.

### Scan: `prov-scan`

The root `WiFiScanPayload` has `msg=1`, `status=2`, and these `oneof` fields:

| Operation | Request `msg` / field | Response `msg` / field |
|---|---|---|
| Start | 0 / `cmd_scan_start=10` | 1 / `resp_scan_start=11` |
| Status | 2 / `cmd_scan_status=12` | 3 / `resp_scan_status=13` |
| Results | 4 / `cmd_scan_result=14` | 5 / `resp_scan_result=15` |

`CmdScanStart` contains `bool blocking=1`, `bool passive=2`,
`uint32 group_channels=3`, `uint32 period_ms=4`. The original client used
`blocking=true`, `passive=false`, `group_channels=0`, `period_ms=120`.
Those are request parameters, not fixed protocol constants.

Poll with an empty `CmdScanStatus` until `RespScanStatus.scan_finished` (field 1)
is true. Field 2, `result_count`, is the number of results available.
Request pages with `CmdScanResult.start_index=1` and `count=2` (both `uint32`).
`RespScanResult.entries=1` is a repeated `WiFiScanResult`:

| Field | Type / number |
|---|---|
| `ssid` | `bytes`, 1 |
| `channel` | `uint32`, 2 |
| `rssi` | `int32`, 3, dBm |
| `bssid` | `bytes`, 4, six-byte address |
| `auth` | `WifiAuthMode`, 5 |

Pages of four are a client convention. Bound requests by `result_count` and
the response size the transport can carry. IDF caps the result list through
`CONFIG_WIFI_PROV_SCAN_MAX_ENTRIES` (default 16 in the source baseline), so the
count need not include every AP on the air. Check the root `status` on every reply.

### Configure: `prov-config`

The root `WiFiConfigPayload` has `msg=1` and these `oneof` fields. Status lives
in the individual response message, not in the root envelope.

| Operation | Request `msg` / field | Response `msg` / field |
|---|---|---|
| Status | 0 / `cmd_get_status=10` | 1 / `resp_get_status=11` |
| Set credentials | 2 / `cmd_set_config=12` | 3 / `resp_set_config=13` |
| Apply | 4 / `cmd_apply_config=14` | 5 / `resp_apply_config=15` |

`CmdSetConfig` contains `bytes ssid=1`, `bytes passphrase=2`, `bytes bssid=3`,
and `int32 channel=4`. The v5.4.3 handler accepts SSIDs of **up to 32 bytes**,
passphrases of **up to 63 bytes**, and BSSID lengths of **0 or 6 bytes**.
These are byte lengths, not character counts. Omit/empty the passphrase for an
open network. BSSID and channel may be omitted.

Check `RespSetConfig.status=1` before sending the empty `CmdApplyConfig`, then
check `RespApplyConfig.status=1`. Applying credentials starts the connection
attempt; it does not confirm a connection.

Poll using the empty `CmdGetStatus`. The response is:

| `RespGetStatus` field | Type / number |
|---|---|
| `status` | `Status`, 1 |
| `sta_state` | `WifiStationState`, 2 |
| `fail_reason` | `WifiConnectFailedReason`, oneof field 10 |
| `connected` | `WifiConnectedState`, oneof field 11 |
| `attempt_failed` | `WifiAttemptFailed`, oneof field 12 |

`WifiStationState` values are `Connected=0`, `Connecting=1`, `Disconnected=2`,
`ConnectionFailed=3`. Check `status` and the response branch before interpreting
zero as Connected. Failure reasons are `AuthError=0` and `NetworkNotFound=1`.
The IDF handler maps a failed station connection to `ConnectionFailed`; clients
should also handle unexpected/unsupported states explicitly.

`WifiAttemptFailed.attempts_remaining=1` (`uint32`) is retry information while
the state is Connecting, not a terminal failure. Older protobuf bindings may
ignore this field and continue polling the state.

`WifiConnectedState` contains `string ip4_addr=1`, `WifiAuthMode auth_mode=2`,
`bytes ssid=3`, `bytes bssid=4`, `int32 channel=5`. Authentication-mode values are
Open 0, WEP 1, WPA_PSK 2, WPA2_PSK 3, WPA_WPA2_PSK 4, WPA2_ENTERPRISE 5,
WPA3_PSK 6, and WPA2_WPA3_PSK 7 in this baseline. Tolerate newer values.

### Control: `prov-ctrl`

The root `WiFiCtrlPayload` has `msg=1`, `status=2`, and:

| Operation | Request `msg` / field | Response `msg` / field |
|---|---|---|
| Reset | 1 / `cmd_ctrl_reset=11` | 2 / `resp_ctrl_reset=12` |
| Reprovision | 3 / `cmd_ctrl_reprov=13` | 4 / `resp_ctrl_reprov=14` |

All four nested messages are empty; the root response status carries the result.
Reset calls the manager's reset-on-failure operation and requires its failure
state. Reprovision requires an initialized manager with auto-stop disabled
(the library disables it); v5.4.3 does not explicitly require a completed
provisioning state. It clears the manager's current Wi-Fi configuration,
disconnects Wi-Fi, and returns the manager to Started if those operations succeed.
Check the root status in either case; the library's automatic reset after failed
attempts may race a manual reset.

These operations reset the provisioning manager's state/configuration. They are
**not** a factory reset of the library's multi-network and custom-variable stores.

## End-to-end client flow {#client-flow}

1. Discover the device and endpoint names, then read `proto-ver`.
2. Validate/select the security scheme and complete its handshake.
3. Optionally scan, waiting for completion and reading bounded result pages.
4. Set credentials, check success, apply, and check success again.
5. Poll connection status until confirmed Connected, an explicit failure, or
   the client's deadline. Choose an interval compatible with the firmware's
   configured teardown/reboot window; a five-second SDK polling interval is
   not a protocol requirement.
6. If needed, read `esp-wifi-config-network-info` on the **same connection**
   before disconnecting. Retry `connected:false` only within that window.
7. Disconnect after the required reads are complete.

If BLE drops before confirmed connection, report an **unconfirmed outcome** and
verify through an application-specific LAN/status check. A disconnect or the
device disappearing from advertising is not proof that credentials worked.
See [reboot and connection handling](./ble-protocol.md#connection-lifecycle).

Each new BLE connection needs discovery and a fresh handshake. Do not resume a
CTR stream or GCM counter from a previous connection. After an uncertain encrypted
exchange, reconnect instead of replaying a request on the old session. During
the library's NimBLE restart workaround, allow a bounded retry/backoff while the
device resumes advertising.

## Python transport recipe {#python-client}

The repository includes a [working CLI and vendored protocol modules](https://github.com/WiFiConfig/esp_wifi_config/tree/d788922/tools/wifi_ble_cli).
The example below shows where to place serialization and the session lock when
building another Bleak-based client. `client` is an already connected client;
`service` is its discovered provisioning service. `encrypt` and `decrypt` are
the established session's byte-to-byte functions.

```python
import asyncio


async def discover_endpoints(client, service):
    endpoints = {}
    description_uuid = "00002901-0000-1000-8000-00805f9b34fb"
    for characteristic in service.characteristics:
        for descriptor in characteristic.descriptors:
            if descriptor.uuid.lower() == description_uuid:
                name = (await client.read_gatt_descriptor(descriptor.handle)).decode("utf-8")
                endpoints[name] = characteristic
    return endpoints


class EndpointSession:
    def __init__(self, client, endpoints, encrypt, decrypt):
        self.client, self.endpoints = client, endpoints
        self.encrypt, self.decrypt = encrypt, decrypt
        self.lock = asyncio.Lock()

    async def transact(self, name, plaintext):
        characteristic = self.endpoints[name]  # missing endpoint is an error
        async with self.lock:
            plain_endpoint = name in {"proto-ver", "prov-session"}
            payload = plaintext if plain_endpoint else self.encrypt(plaintext)
            if not payload:
                raise ValueError("Use a nonempty endpoint request")
            await self.client.write_gatt_char(characteristic, payload, response=True)
            response = bytes(await self.client.read_gatt_char(characteristic))
            return response if plain_endpoint else self.decrypt(response)
```

The caller owns deadlines, cancellation, response validation, and reconnection
after errors. Reuse one session object for all endpoints on the connection.
Do not call Bleak's private backend methods to force MTU negotiation; use the
supported transport behavior of the deployed Bleak/platform version.
