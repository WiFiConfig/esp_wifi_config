#pragma once

#include "sdkconfig.h"

// These options configure this library's sources, never the precompiled SDK.
// Arduino build_opt.h / PlatformIO build_flags apply them to every source file.
#ifdef ARDUINO_ARCH_ESP32
#include "soc/soc_caps.h"
#include "esp_arduino_version.h"
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 11)
#error "esp_wifi_config requires Arduino-ESP32 3.3.11 or newer"
#endif
#if !SOC_WIFI_SUPPORTED
#error "esp_wifi_config requires a chip with native Wi-Fi"
#endif
#ifndef WIFI_CFG_ARDUINO_SOFTAP
#define WIFI_CFG_ARDUINO_SOFTAP 1
#endif
#ifndef WIFI_CFG_ARDUINO_WEBUI
#define WIFI_CFG_ARDUINO_WEBUI WIFI_CFG_ARDUINO_SOFTAP
#endif
#ifndef WIFI_CFG_ARDUINO_PROV_BLE
#define WIFI_CFG_ARDUINO_PROV_BLE 0
#endif
#ifndef WIFI_CFG_ARDUINO_IMPROV_BLE
#define WIFI_CFG_ARDUINO_IMPROV_BLE 0
#endif
#ifndef WIFI_CFG_ARDUINO_IMPROV_SERIAL
#define WIFI_CFG_ARDUINO_IMPROV_SERIAL 0
#endif
#ifndef WIFI_CFG_ARDUINO_NIMBLE
#define WIFI_CFG_ARDUINO_NIMBLE 0
#endif
#define WIFI_CFG_SOFTAP WIFI_CFG_ARDUINO_SOFTAP
#if WIFI_CFG_ARDUINO_WEBUI && !WIFI_CFG_ARDUINO_SOFTAP
#error "The web UI requires WIFI_CFG_ARDUINO_SOFTAP=1"
#endif
#if WIFI_CFG_ARDUINO_WEBUI
#define CONFIG_WIFI_CFG_ENABLE_WEBUI 1
#define WIFI_CFG_WEBUI_EMBEDDED 1
#endif
#if WIFI_CFG_ARDUINO_PROV_BLE && WIFI_CFG_ARDUINO_IMPROV_BLE
#error "Choose either ESP provisioning BLE or Improv BLE"
#endif
#if WIFI_CFG_ARDUINO_PROV_BLE || WIFI_CFG_ARDUINO_IMPROV_BLE
#if !SOC_BLE_SUPPORTED || (!WIFI_CFG_ARDUINO_NIMBLE && !defined(CONFIG_BT_BLUEDROID_ENABLED) && !defined(CONFIG_BT_NIMBLE_ENABLED))
#error "This Arduino board package does not provide a supported BLE host"
#endif
#endif

#if WIFI_CFG_ARDUINO_PROV_BLE
#define CONFIG_WIFI_CFG_ENABLE_NETWORK_PROVISIONING 1
#define CONFIG_WIFI_CFG_NETWORK_PROVISIONING_BLE 1
#endif
#if WIFI_CFG_ARDUINO_IMPROV_BLE
#define CONFIG_WIFI_CFG_ENABLE_IMPROV_BLE 1
#endif
#if WIFI_CFG_ARDUINO_IMPROV_SERIAL
#define CONFIG_WIFI_CFG_ENABLE_IMPROV_SERIAL 1
#endif
#ifdef CONFIG_WIFI_CFG_ENABLE_CLI
#error "The esp_console CLI is available in ESP-IDF builds only"
#endif
#endif

// IDF builds always use their configured SDK host.
#ifndef WIFI_CFG_ARDUINO_NIMBLE
#define WIFI_CFG_ARDUINO_NIMBLE 0
#endif
