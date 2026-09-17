/* [新增] 主機替身；只驗證應用狀態邏輯，不模擬 RF / FreeRTOS 排程。 */
#pragma once
#include <Arduino.h>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#ifndef BLE_HS_EDONE
#define BLE_HS_EDONE 14
#endif

namespace NIMBLE_PROPERTY {
enum { READ = 1, WRITE = 2, NOTIFY = 4, INDICATE = 8 };
}

struct NimBLEConnInfo {
    uint16_t handle = 1U;
    uint16_t getConnHandle() const { return handle; }
};
class NimBLECharacteristic;
class NimBLEServer;
class NimBLEServerCallbacks {
public:
    virtual ~NimBLEServerCallbacks() = default;
    virtual void onConnect(NimBLEServer *, NimBLEConnInfo &) {}
    virtual void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) {}
};
class NimBLECharacteristicCallbacks {
public:
    virtual ~NimBLECharacteristicCallbacks() = default;
    virtual void onRead(NimBLECharacteristic *, NimBLEConnInfo &) {}
    virtual void onWrite(NimBLECharacteristic *, NimBLEConnInfo &) {}
    virtual void onStatus(NimBLECharacteristic *, NimBLEConnInfo &, int) {}
    virtual void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &, uint16_t) {}
};
struct NimBLEAttValue {
    std::vector<uint8_t> bytes;
    size_t size() const { return bytes.size(); }
    const uint8_t *data() const { return bytes.data(); }
};
struct MockNotification {
    uint32_t time_ms;
    uint16_t handle;
    std::vector<uint8_t> bytes;
};
inline std::vector<MockNotification> mock_notifications;
inline std::vector<MockNotification> mock_indications;
inline bool mock_notify_failure = false;
inline bool mock_indicate_failure = false;
inline bool mock_indicate_no_confirm = false;
inline bool mock_init_failure = false;
inline bool mock_adv_failure = false;
inline bool mock_deinit_failure = false;
inline uint32_t mock_init_calls = 0U;
inline uint32_t mock_deinit_calls = 0U;
inline uint32_t mock_notify_attempts = 0U;
inline uint32_t mock_indicate_attempts = 0U;

class NimBLECharacteristic {
public:
    std::string uuid;
    uint32_t properties;
    size_t limit;
    NimBLECharacteristicCallbacks *callback = nullptr;
    NimBLEAttValue value;
    NimBLECharacteristic(std::string id, uint32_t props, size_t max_size)
        : uuid(std::move(id)), properties(props), limit(max_size) {}
    void setCallbacks(NimBLECharacteristicCallbacks *cb) { callback = cb; }
    void setValue(const uint8_t *data, size_t size) {
        assert(size <= limit);
        value.bytes.assign(data, data + size);
    }
    void setValue(const char *str) {
        std::string text_value(str);
        setValue(reinterpret_cast<const uint8_t *>(text_value.data()), text_value.size());
    }
    NimBLEAttValue getValue() const { return value; }
    bool notify(const uint8_t *data, size_t size, uint16_t handle) const {
        assert(size <= 20U);
        mock_notify_attempts++;
        if (mock_notify_failure) return false;
        mock_notifications.push_back({millis(), handle, std::vector<uint8_t>(data, data + size)});
        return true;
    }
    bool indicate(const uint8_t *data, size_t size, uint16_t handle) const {
        assert(size <= 20U);
        mock_indicate_attempts++;
        if (mock_indicate_failure) return false;
        mock_indications.push_back({millis(), handle, std::vector<uint8_t>(data, data + size)});
        if (!mock_indicate_no_confirm && callback) {
            NimBLEConnInfo info{handle};
            callback->onStatus(const_cast<NimBLECharacteristic *>(this), info, BLE_HS_EDONE);
        }
        return true;
    }
};
class NimBLEService {
public:
    std::vector<std::unique_ptr<NimBLECharacteristic>> characteristics;
    NimBLECharacteristic *createCharacteristic(const char *uuid, uint32_t properties, uint16_t max_size) {
        characteristics.push_back(std::make_unique<NimBLECharacteristic>(uuid, properties, max_size));
        return characteristics.back().get();
    }
};
class NimBLEServer {
public:
    NimBLEServerCallbacks *callback = nullptr;
    bool auto_advertise = true;
    std::vector<std::unique_ptr<NimBLEService>> services;
    void setCallbacks(NimBLEServerCallbacks *cb, bool owned) { assert(!owned); callback = cb; }
    void advertiseOnDisconnect(bool enabled) { auto_advertise = enabled; }
    NimBLEService *createService(const char *) {
        services.push_back(std::make_unique<NimBLEService>());
        return services.back().get();
    }
    bool start() { return true; }
    bool disconnect(uint16_t handle) const {
        NimBLEConnInfo info{handle};
        if (callback) callback->onDisconnect(const_cast<NimBLEServer *>(this), info, 0);
        return true;
    }
};
class NimBLEAdvertising {
public:
    bool advertising = false;
    std::string name, uuid;
    void enableScanResponse(bool) {}
    bool addServiceUUID(const char *value) { uuid = value; return true; }
    bool setName(const std::string &value) { name = value; return true; }
    void setMinInterval(uint16_t) {}
    void setMaxInterval(uint16_t) {}
    bool start() { advertising = !mock_adv_failure; return advertising; }
    bool stop() { advertising = false; return true; }
    bool isAdvertising() const { return advertising; }
};
class NimBLEDevice {
public:
    inline static bool initialized = false;
    inline static std::unique_ptr<NimBLEServer> server;
    inline static NimBLEAdvertising advertising;
    static bool init(const std::string &) {
        mock_init_calls++;
        initialized = !mock_init_failure;
        return initialized;
    }
    static bool isInitialized() { return initialized; }
    static bool deinit(bool clear_all) {
        if (initialized) {
            mock_deinit_calls++;
            if (mock_deinit_failure) {
                assert(!clear_all); // 停止失敗時不可釋放物件。
                return false;
            }
        }
        advertising = {};
        if (clear_all) server.reset();
        initialized = false;
        return true;
    }
    static bool setPower(int8_t dbm) { return dbm == -6; }
    static bool setMTU(uint16_t mtu) { return mtu == 23; }
    static NimBLEServer *createServer() {
        server = std::make_unique<NimBLEServer>();
        return server.get();
    }
    static NimBLEAdvertising *getAdvertising() { return &advertising; }
};
