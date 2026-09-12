#!/usr/bin/env python3
"""Exercise the production Arduino adapter's ownership/failure paths on the host.

SDK/sketch compilation is covered separately by the real Arduino builds. These
fakes expose resource mutations so failure paths can be checked without a radio.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

HEADER = r'''
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
using esp_err_t = int;
using esp_event_base_t = const char *;
using esp_event_handler_t = void (*)(void *, esp_event_base_t, int32_t, void *);
using wifi_mode_t = int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_TIMEOUT=1, ESP_ERR_INVALID_ARG=2,
              ESP_ERR_INVALID_STATE=3, ESP_ERR_NOT_SUPPORTED=4;
constexpr int WIFI_MODE_NULL=0, WIFI_STATE_CONNECTED=2, ESP_EVENT_ANY_ID=-1;
constexpr const char *WIFI_CFG_EVENT="wifi_cfg";
struct wifi_cfg_config_t { bool enable_ap=false; };
struct wifi_status_t { int state=0; };
class Stream {
public:
    virtual int read() = 0;
    virtual size_t write(const uint8_t *, size_t) = 0;
    virtual ~Stream() = default;
};
void delay(unsigned);
wifi_cfg_config_t wifi_cfg_default_config();
int wifi_cfg_init(const wifi_cfg_config_t *);
int wifi_cfg_deinit(bool);
int wifi_cfg_get_status(wifi_status_t *);
int esp_event_loop_create_default();
int esp_event_handler_register(const char *, int, esp_event_handler_t, void *);
int esp_event_handler_unregister(const char *, int, esp_event_handler_t);
class ESPWiFiConfig {
public:
    static wifi_cfg_config_t defaults() { return wifi_cfg_default_config(); }
    esp_err_t begin(const wifi_cfg_config_t &);
    esp_err_t begin();
    esp_err_t end(bool=false);
    bool connected() const;
    wifi_status_t status() const;
    esp_err_t onEvent(esp_event_handler_t, void * = nullptr);
    esp_err_t removeEvent(esp_event_handler_t);
    esp_err_t setImprovSerial(Stream &);
};
extern "C" {
int wifi_cfg_platform_init();
int wifi_cfg_platform_set_mode(int);
void wifi_cfg_platform_release(bool);
bool wifi_cfg_arduino_serial_ready();
int wifi_cfg_arduino_serial_read(uint8_t *);
void wifi_cfg_arduino_serial_write(const uint8_t *, size_t);
}
struct FakeSTA {
    bool begin_ok=true, disconnect_ok=true;
    bool tried_connect=true, erased=true;
    int begins=0, disconnects=0;
    bool begin(bool connect) { tried_connect=connect; ++begins; return begin_ok; }
    bool disconnect(bool erase, unsigned) { erased=erase; ++disconnects; return disconnect_ok; }
};
struct FakeWiFi {
    FakeSTA STA;
    bool reconnect=true, mode_ok=true;
    int changes=0, selected_mode=1;
    bool getAutoReconnect() { return reconnect; }
    void setAutoReconnect(bool value) { reconnect=value; }
    bool mode(int value) { ++changes; selected_mode=value; return mode_ok; }
};
extern FakeWiFi WiFi;
'''

TEST = r'''
#include "fake.h"
#include <cassert>
#include <cstdio>
FakeWiFi WiFi;
void delay(unsigned) {}
wifi_cfg_config_t wifi_cfg_default_config() { return {}; }
int wifi_cfg_init(const wifi_cfg_config_t *) { return 0; }
int wifi_cfg_deinit(bool) { return 0; }
int wifi_cfg_get_status(wifi_status_t *) { return 0; }
int esp_event_loop_create_default() { return 0; }
int esp_event_handler_register(const char *, int, esp_event_handler_t, void *) { return 0; }
int esp_event_handler_unregister(const char *, int, esp_event_handler_t) { return 0; }
struct TestStream : Stream {
    std::string written;
    int read() override { return 0x42; }
    size_t write(const uint8_t *p, size_t n) override { written.append((const char *)p,n); return n; }
};
int main() {
    // Releasing an inactive adapter must not turn off someone else's Wi-Fi.
    wifi_cfg_platform_release(true);
    assert(WiFi.changes==0 && WiFi.reconnect);
    assert(wifi_cfg_platform_init()==ESP_OK);
    assert(!WiFi.reconnect && !WiFi.STA.tried_connect && !WiFi.STA.erased);
    wifi_cfg_platform_release(false);
    assert(WiFi.changes==0 && WiFi.reconnect);
    // Preserve an application that already disabled its own reconnect policy.
    WiFi.reconnect=false;
    assert(wifi_cfg_platform_init()==ESP_OK);
    wifi_cfg_platform_release(true);
    assert(!WiFi.reconnect && WiFi.selected_mode==WIFI_MODE_NULL && WiFi.changes==1);
    // Both initialization failures restore the policy and leave ownership alone.
    WiFi=FakeWiFi{};
    WiFi.STA.begin_ok=false;
    assert(wifi_cfg_platform_init()==ESP_FAIL && WiFi.reconnect);
    assert(WiFi.STA.disconnects==0);
    wifi_cfg_platform_release(true);
    assert(WiFi.changes==0);
    WiFi=FakeWiFi{};
    WiFi.STA.disconnect_ok=false;
    assert(wifi_cfg_platform_init()==ESP_ERR_TIMEOUT && WiFi.reconnect);
    wifi_cfg_platform_release(true);
    assert(WiFi.changes==0);
    // Mode failures propagate; stream replacement is refused during a session.
    WiFi=FakeWiFi{};
    WiFi.mode_ok=false;
    assert(wifi_cfg_platform_set_mode(3)==ESP_FAIL);
    ESPWiFiConfig config;
    TestStream stream, other;
    assert(config.setImprovSerial(stream)==ESP_OK);
    assert(wifi_cfg_platform_init()==ESP_OK);
    assert(config.setImprovSerial(other)==ESP_ERR_INVALID_STATE);
    uint8_t byte=0;
    assert(wifi_cfg_arduino_serial_read(&byte)==1 && byte==0x42);
    wifi_cfg_arduino_serial_write(&byte,1);
    assert(stream.written=="B" && other.written.empty());
    wifi_cfg_platform_release(false);
    puts("Arduino adapter ownership, rollback and stream checks passed");
}
'''

with tempfile.TemporaryDirectory(prefix="wifi-arduino-test-") as temp:
    work = Path(temp)
    # Copy without editing so quoted includes bind to the fake SDK, not src/.
    shutil.copy2(ROOT / "src/ESPWiFiConfig.cpp", work / "adapter.cpp")
    (work / "fake.h").write_text(HEADER)
    for name in ["ESPWiFiConfig.h", "esp_wifi_config_platform.h", "WiFi.h"]:
        (work / name).write_text('#include "fake.h"\n')
    (work / "esp_wifi_config_build.h").write_text('''#pragma once
#define ARDUINO_ARCH_ESP32 1
#define WIFI_CFG_ARDUINO_SOFTAP 1
#define WIFI_CFG_ARDUINO_IMPROV_SERIAL 1
#define WIFI_CFG_ARDUINO_PROV_BLE 0
#define WIFI_CFG_ARDUINO_IMPROV_BLE 0
''')
    (work / "test.cpp").write_text(TEST)
    binary = work / "test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(work),
                    str(work / "adapter.cpp"), str(work / "test.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
