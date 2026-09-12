// Optional h2zero/NimBLE-Arduino transport. The SDK provisioning manager still
// owns RPCs and Security 0/1/2; only the BLE host and GATT transport change.
#include "esp_wifi_config_build.h"
#if defined(ARDUINO_ARCH_ESP32) && WIFI_CFG_ARDUINO_NIMBLE && \
    (WIFI_CFG_ARDUINO_PROV_BLE || WIFI_CFG_ARDUINO_IMPROV_BLE)

#include <NimBLEDevice.h>
#include <NimBLECppVersion.h>
#if NIMBLE_CPP_VERSION < NIMBLE_CPP_VERSION_VAL(2, 5, 1)
#error "esp_wifi_config requires NimBLE-Arduino 2.5.1 or newer"
#endif
#include "esp_wifi_config_nimble_arduino.h"
#include "esp_wifi_config_ble_int.h"
#include "esp_log.h"
#include "esp_bt.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#if WIFI_CFG_ARDUINO_PROV_BLE
#include "protocomm.h"
#include "protocomm_ble.h"
ESP_EVENT_DEFINE_BASE(WIFI_CFG_NIMBLE_PROV_EVENT);
#else
#include "esp_wifi_config_improv.h"
#endif

namespace {
const char *const TAG = "wifi_cfg_nimble_arduino";
NimBLEServer *server = nullptr;
NimBLEService *service = nullptr;
std::atomic<uint16_t> connection{BLE_HS_CONN_HANDLE_NONE};
std::atomic<bool> advertising{false};
// Notifications originate on the Improv worker. Serialize them with teardown.
std::mutex lifecycle;

#if WIFI_CFG_ARDUINO_PROV_BLE
protocomm_t *protocomm = nullptr;
struct Endpoint {
    std::string name;
    uint16_t uuid;
};
struct ProvConfig {
    std::string name;
    std::vector<Endpoint> endpoints;
};
uint8_t service_uuid[16] = {
    0x07, 0xed, 0x9b, 0x2d, 0x0f, 0x06, 0x7c, 0x87,
    0x9b, 0x43, 0x43, 0x6b, 0x4d, 0x24, 0x75, 0x17,
};
std::vector<uint8_t> manufacturer;

class EndpointCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit EndpointCallbacks(const std::string &name) : name_(name) {}
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        if (!protocomm || connection != info.getConnHandle()) return;
        const auto request = characteristic->getValue();
        uint8_t *response = nullptr;
        ssize_t length = 0;
        esp_err_t err = protocomm_req_handle(protocomm, name_.c_str(), info.getConnHandle(),
                                            request.data(), request.size(), &response, &length);
        // Never leave a previous response (or a just-written request) readable
        // after a rejected security handshake / malformed request.
        characteristic->setValue(static_cast<const uint8_t *>(nullptr), 0);
        if (err == ESP_OK && length >= 0 && length <= BLE_ATT_ATTR_MAX_LEN && (!length || response)) {
            characteristic->setValue(response, length);
        } else {
            ESP_LOGW(TAG, "%s rejected: %s (response %d bytes)", name_.c_str(), esp_err_to_name(err), int(length));
            server->disconnect(info.getConnHandle());
        }
        free(response);
    }
private:
    std::string name_;
};
std::vector<std::unique_ptr<EndpointCallbacks>> endpoints;
std::vector<NimBLECharacteristic *> characteristics;
#else
NimBLECharacteristic *state_char = nullptr;
NimBLECharacteristic *error_char = nullptr;
NimBLECharacteristic *result_char = nullptr;
uint8_t improv_uuid[16] = IMPROV_BLE_SVC_UUID_128;
std::string improv_name;
bool configure_advertising(const uint8_t *uuid, const std::string &name);

class ImprovCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        uint8_t value;
        if (characteristic == state_char) value = wifi_cfg_improv_get_state();
        else if (characteristic == error_char) value = wifi_cfg_improv_get_error();
        else return;
        characteristic->setValue(&value, 1);
    }
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        if (connection != info.getConnHandle()) return;
        const auto value = characteristic->getValue();
        if (wifi_cfg_nimble_improv_enqueue(value.data(), value.size()) != ESP_OK) {
            server->disconnect(info.getConnHandle());
        }
        // Credentials are consumed by the queue; don't retain them in GATT.
        characteristic->setValue(static_cast<const uint8_t *>(nullptr), 0);
    }
} improv_callbacks;
#endif

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *srv, NimBLEConnInfo &info) override {
        uint16_t empty = BLE_HS_CONN_HANDLE_NONE;
        if (!advertising || !connection.compare_exchange_strong(empty, info.getConnHandle())) {
            srv->disconnect(info.getConnHandle());
            return;
        }
        // One client owns the characteristic responses and security session.
        NimBLEDevice::getAdvertising()->stop();
#if WIFI_CFG_ARDUINO_PROV_BLE
        if (protocomm_open_session(protocomm, info.getConnHandle()) != ESP_OK) {
            srv->disconnect(info.getConnHandle());
        }
#endif
    }
    void onDisconnect(NimBLEServer *, NimBLEConnInfo &info, int reason) override {
        if (connection != info.getConnHandle()) return;
#if WIFI_CFG_ARDUINO_PROV_BLE
        if (protocomm) protocomm_close_session(protocomm, info.getConnHandle());
        for (auto *characteristic : characteristics) {
            characteristic->setValue(static_cast<const uint8_t *>(nullptr), 0);
        }
#endif
        connection = BLE_HS_CONN_HANDLE_NONE;
        if (!advertising) return;
#if WIFI_CFG_ARDUINO_PROV_BLE
        protocomm_ble_event_t event = {};
        event.evt_type = PROTOCOMM_TRANSPORT_BLE_DISCONNECTED;
        event.conn_handle = info.getConnHandle();
        event.disconnect_reason = reason;
        // A private event base avoids pulling the SDK's BLE transport archive.
        esp_event_post(WIFI_CFG_NIMBLE_PROV_EVENT, PROTOCOMM_TRANSPORT_BLE_DISCONNECTED,
                       &event, sizeof(event), 0);
#else
        // Refresh the state published in Improv's discovery service data.
        if (!configure_advertising(improv_uuid, improv_name)) return;
#endif
        NimBLEDevice::getAdvertising()->start();
    }
} server_callbacks;

esp_err_t init_host(const char *name) {
    // Don't replace another application's singleton server or its callbacks.
    if (server || NimBLEDevice::isInitialized() ||
        esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {
        ESP_LOGE(TAG, "NimBLE-Arduino provisioning requires exclusive ownership of the BLE stack");
        return ESP_ERR_INVALID_STATE;
    }
    if (!NimBLEDevice::init(name)) return ESP_FAIL;
    NimBLEDevice::setMTU(517);
    server = NimBLEDevice::createServer();
    if (!server) {
        NimBLEDevice::deinit(true);
        return ESP_ERR_NO_MEM;
    }
    server->setCallbacks(&server_callbacks, false);
    server->advertiseOnDisconnect(false);
    connection = BLE_HS_CONN_HANDLE_NONE;
    ESP_LOGI(TAG, "%s", NIMBLE_CPP_VERSION_STR);
    return ESP_OK;
}

esp_err_t stop_advertising() {
    advertising = false;
    if (!server) return ESP_OK;
    NimBLEDevice::getAdvertising()->stop();
    if (connection != BLE_HS_CONN_HANDLE_NONE) server->disconnect(connection);
    return ESP_OK;
}

esp_err_t deinit_host() {
    std::lock_guard<std::mutex> lock(lifecycle);
    stop_advertising();
    if (!server) return ESP_OK;
    // Join the host before releasing callbacks or the manager's protocomm.
    // deinit(false) leaves objects intact if stopping the host fails.
    if (!NimBLEDevice::deinit(false)) return ESP_FAIL;
#if WIFI_CFG_ARDUINO_PROV_BLE
    if (connection != BLE_HS_CONN_HANDLE_NONE && protocomm) {
        protocomm_close_session(protocomm, connection);
    }
#endif
    NimBLEDevice::deinit(true);
    server = nullptr;
    service = nullptr;
    connection = BLE_HS_CONN_HANDLE_NONE;
#if WIFI_CFG_ARDUINO_PROV_BLE
    protocomm = nullptr;
    endpoints.clear();
    characteristics.clear();
#else
    state_char = error_char = result_char = nullptr;
#endif
    return ESP_OK;
}

bool configure_advertising(const uint8_t *uuid, const std::string &name) {
    auto *adv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData primary, scan;
    bool ok = primary.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP) &&
              primary.addServiceUUID(NimBLEUUID(uuid, 16));
    // Legacy scan response: 31 bytes, with a two-byte name field header.
    std::string short_name = name.substr(0, 29);
    ok = ok && scan.setName(short_name, short_name.size() == name.size());
#if WIFI_CFG_ARDUINO_PROV_BLE
    if (!manufacturer.empty()) {
        size_t available = 31 - short_name.size() - 2;
        if (available < manufacturer.size() + 2) {
            ESP_LOGE(TAG, "Device name and manufacturer data exceed the 31-byte scan response");
            return false;
        }
        ok = ok && scan.setManufacturerData(manufacturer.data(), manufacturer.size());
    }
#else
    const uint8_t data[] = {uint8_t(wifi_cfg_improv_get_state()), wifi_cfg_improv_get_capabilities(), 0, 0, 0, 0};
    ok = ok && primary.setServiceData(NimBLEUUID(uint16_t(0x4677)), data, sizeof(data));
#endif
    adv->enableScanResponse(true);
    return ok && adv->setAdvertisementData(primary) && adv->setScanResponseData(scan);
}

#if WIFI_CFG_ARDUINO_PROV_BLE
esp_err_t prov_start(protocomm_t *pc, void *opaque) {
    if (!pc || !opaque) return ESP_ERR_INVALID_ARG;
    auto *config = static_cast<ProvConfig *>(opaque);
    esp_err_t err = init_host(config->name.c_str());
    if (err != ESP_OK) return err;
    protocomm = pc;
    service = server->createService(NimBLEUUID(service_uuid, sizeof(service_uuid)));
    if (service) {
        for (const auto &endpoint : config->endpoints) {
            uint8_t uuid[16];
            memcpy(uuid, service_uuid, sizeof(uuid));
            uuid[12] = endpoint.uuid & 0xff;
            uuid[13] = endpoint.uuid >> 8;
            auto *characteristic = service->createCharacteristic(NimBLEUUID(uuid, sizeof(uuid)),
                                                                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
            if (!characteristic) break;
            auto *descriptor = characteristic->createDescriptor("2901", NIMBLE_PROPERTY::READ);
            if (!descriptor) break;
            descriptor->setValue(endpoint.name);
            auto callback = std::unique_ptr<EndpointCallbacks>(new (std::nothrow) EndpointCallbacks(endpoint.name));
            if (!callback) break;
            characteristic->setCallbacks(callback.get());
            endpoints.push_back(std::move(callback));
            characteristics.push_back(characteristic);
        }
    }
    if (!service || characteristics.size() != config->endpoints.size() || !server->start() ||
        !configure_advertising(service_uuid, config->name)) {
        deinit_host();
        return ESP_FAIL;
    }
    advertising = true;
    if (!NimBLEDevice::getAdvertising()->start()) {
        deinit_host();
        return ESP_FAIL;
    }
    return ESP_OK;
}
esp_err_t prov_stop(protocomm_t *) { return deinit_host(); }
void *new_config() { return new (std::nothrow) ProvConfig; }
void delete_config(void *config) { delete static_cast<ProvConfig *>(config); }
esp_err_t set_service(void *opaque, const char *name, const char *) {
    if (!opaque || !name) return ESP_ERR_INVALID_ARG;
    static_cast<ProvConfig *>(opaque)->name = name;
    return ESP_OK;
}
esp_err_t set_endpoint(void *opaque, const char *name, uint16_t uuid) {
    if (!opaque || !name) return ESP_ERR_INVALID_ARG;
    static_cast<ProvConfig *>(opaque)->endpoints.push_back({name, uuid});
    return ESP_OK;
}
#endif
} // namespace

#if WIFI_CFG_ARDUINO_PROV_BLE
const network_prov_scheme_t wifi_cfg_nimble_prov_scheme = {
    prov_start, prov_stop, new_config, delete_config, set_service, set_endpoint, WIFI_MODE_STA
};
esp_err_t wifi_cfg_nimble_prov_set_uuid(uint8_t *uuid) {
    const uint8_t default_uuid[] = {
        0x07, 0xed, 0x9b, 0x2d, 0x0f, 0x06, 0x7c, 0x87,
        0x9b, 0x43, 0x43, 0x6b, 0x4d, 0x24, 0x75, 0x17,
    };
    memcpy(service_uuid, uuid ? uuid : default_uuid, sizeof(service_uuid));
    return ESP_OK;
}
esp_err_t wifi_cfg_nimble_prov_set_mfg_data(uint8_t *data, ssize_t length) {
    if (length < 0 || (!data && length)) return ESP_ERR_INVALID_ARG;
    manufacturer.clear();
    if (length) manufacturer.assign(data, data + length);
    return ESP_OK;
}
#else
esp_err_t wifi_cfg_ble_backend_init(const char *name) {
    esp_err_t err = init_host(name);
    if (err != ESP_OK) return err;
    improv_name = name;
    service = server->createService(NimBLEUUID(improv_uuid, sizeof(improv_uuid)));
    NimBLECharacteristic *chars[5] = {};
    const uint32_t properties[] = {
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        NIMBLE_PROPERTY::READ,
    };
    if (service) {
        for (unsigned i = 0; i < 5; ++i) {
            uint8_t uuid[16];
            memcpy(uuid, improv_uuid, sizeof(uuid));
            uuid[0] = i + 1;
            chars[i] = service->createCharacteristic(NimBLEUUID(uuid, sizeof(uuid)), properties[i]);
            if (!chars[i]) break;
            chars[i]->setCallbacks(&improv_callbacks);
        }
    }
    if (!chars[4] || !server->start() || !configure_advertising(improv_uuid, name)) {
        deinit_host();
        return ESP_FAIL;
    }
    state_char = chars[0];
    error_char = chars[1];
    result_char = chars[3];
    uint8_t caps = wifi_cfg_improv_get_capabilities();
    chars[4]->setValue(&caps, 1);
    return ESP_OK;
}
esp_err_t wifi_cfg_ble_backend_deinit(void) { return deinit_host(); }
esp_err_t wifi_cfg_ble_backend_start(void) {
    if (!server) return ESP_ERR_INVALID_STATE;
    advertising = true;
    if (connection != BLE_HS_CONN_HANDLE_NONE) return ESP_OK;
    if (!configure_advertising(improv_uuid, improv_name)) return ESP_FAIL;
    return NimBLEDevice::getAdvertising()->start() ? ESP_OK : ESP_FAIL;
}
esp_err_t wifi_cfg_ble_backend_stop(void) { return stop_advertising(); }
void wifi_cfg_nimble_improv_result(const uint8_t *data, size_t length) {
    std::lock_guard<std::mutex> lock(lifecycle);
    if (!result_char) return;
    result_char->setValue(data, length);
    if (connection != BLE_HS_CONN_HANDLE_NONE) result_char->notify(connection.load());
}
void wifi_cfg_nimble_improv_state(uint8_t state, uint8_t error) {
    std::lock_guard<std::mutex> lock(lifecycle);
    if (!state_char) return;
    state_char->setValue(&state, 1);
    error_char->setValue(&error, 1);
    if (connection == BLE_HS_CONN_HANDLE_NONE) return;
    state_char->notify(connection.load());
    if (error) error_char->notify(connection.load());
}
uint16_t wifi_cfg_nimble_improv_mtu(void) {
    std::lock_guard<std::mutex> lock(lifecycle);
    return server && connection != BLE_HS_CONN_HANDLE_NONE ? server->getPeerMTU(connection) : 23;
}
#endif
#endif
