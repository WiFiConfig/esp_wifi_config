# Arduino validation

`python3 test/arduino/run.py` checks the platform adapter's resource ownership,
failure rollback and Stream selection on the host.

`HardwareInterop/HardwareInterop.ino` is a separate hardware probe for the
Arduino-facing API. It accepts an isolated test AP and HTTP server over serial,
then performs ten connection/client/shutdown cycles. It checks library and
Arduino connection status together, delivers Wi-Fi callbacks, fetches an HTTP
response with `NetworkClient`/`HTTPClient`, alternates `end(false)` and
`end(true)`, and reports restored reconnect settings and free heap. It reports
the actual firmware SHA before its measurements.

Build for `esp32:esp32:esp32s3:PartitionScheme=min_spiffs` with the stock 3.3.11
core. Keep USB CDC on boot disabled: this probe sends its results through the
S3's UART0 bridge while the rig flashes through native USB.

The matching runner is `test_harness/rig/tools/arduino_interop.py` in the separate
test harness repository. Run it on the Raspberry Pi after the ordinary HIL
tests, with `--image` pointing to the sketch's merged image and `--app-sha` to
the descriptor SHA. The runner controls a namespaced test AP, serves the HTTP
response, verifies all ten cycles and AP-side DHCP evidence, and retains raw
serial and radio logs. It accepts less than 8 KiB of heap variation after
warmup, comparing cycles with the same radio state.

The shared boot, reconnect, portal, REST, BLE and Improv suites use the native
Arduino harness builder rather than this probe; see the harness's
`docs/arduino.md` for its build and telemetry wiring options.

`ProvisioningHeap` uses the same configuration as the ordinary ProvisioningBLE
example and only adds heap and firmware identity reports. Build it for
`esp32:esp32:esp32:PartitionScheme=min_spiffs`; the Pi runner
`rig/tools/arduino_ble_example.py --image <merged.bin> --app-sha <descriptor SHA>`
checks Security 1 provisioning and the DUT's DHCP lease without the HIL
telemetry and command tasks. This comparison matters on ESP32 boards without
PSRAM: the measurement firmware itself consumes substantial internal RAM.

`BLEMemory` compares stock BLE with NimBLE-Arduino using the same portal-enabled
application, a restartable memory policy, and three begin/provision/end cycles.
Build it with PROV_BLE or IMPROV_BLE, then with and without
`WIFI_CFG_ARDUINO_NIMBLE=1`. The separate harness runner
`rig/tools/arduino_ble_memory.py` validates identity and real provisioning while
recording heap, minimum heap, largest block and shutdown recovery. Keep core,
board, partition layout, optimization and PSRAM settings identical.
