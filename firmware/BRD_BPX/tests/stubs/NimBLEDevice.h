#ifndef TEST_NIMBLE_H
#define TEST_NIMBLE_H
#include <Arduino.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#define BLE_HS_CONN_HANDLE_NONE 0xFFFFU
namespace NIMBLE_PROPERTY { constexpr uint16_t READ=1U, WRITE=2U, WRITE_NR=4U, NOTIFY=8U; }
extern bool test_notify_ok;
class NimBLECharacteristic;
class NimBLEServer;
class NimBLEConnInfo {
    uint16_t handle_;
public:
    explicit NimBLEConnInfo(uint16_t handle) : handle_(handle) {}
    uint16_t getConnHandle() const { return handle_; }
};
class NimBLECharacteristicCallbacks {
public:
    virtual ~NimBLECharacteristicCallbacks() = default;
    virtual void onWrite(NimBLECharacteristic *,NimBLEConnInfo &) {}
    virtual void onRead(NimBLECharacteristic *,NimBLEConnInfo &) {}
    virtual void onSubscribe(NimBLECharacteristic *,NimBLEConnInfo &,uint16_t) {}
};
class NimBLEServerCallbacks {
public:
    virtual ~NimBLEServerCallbacks() = default;
    virtual void onConnect(NimBLEServer *,NimBLEConnInfo &) {}
    virtual void onDisconnect(NimBLEServer *,NimBLEConnInfo &,int) {}
};
class NimBLECharacteristic {
    NimBLECharacteristicCallbacks *callbacks_=nullptr;
    std::vector<uint8_t> value_;
public:
    mutable std::vector<std::vector<uint8_t>> sent;
    void setCallbacks(NimBLECharacteristicCallbacks *callbacks) { callbacks_=callbacks; }
    NimBLECharacteristicCallbacks *getCallbacks() { return callbacks_; }
    std::vector<uint8_t> getValue() const { return value_; }
    void setValue(const uint8_t *bytes,size_t size) { value_.assign(bytes,bytes+size); }
    bool notify(const uint8_t *bytes,size_t size,uint16_t) const {
        if(!test_notify_ok) { return false; }
        sent.emplace_back(bytes,bytes+size); return true;
    }
};
class NimBLEService {
    std::map<std::string,std::unique_ptr<NimBLECharacteristic>> characteristics_;
public:
    NimBLECharacteristic *createCharacteristic(const char *uuid,uint16_t,size_t) {
        auto characteristic=std::make_unique<NimBLECharacteristic>();
        auto *result=characteristic.get(); characteristics_[uuid]=std::move(characteristic); return result;
    }
    NimBLECharacteristic *getCharacteristic(const char *uuid) { return characteristics_.at(uuid).get(); }
};
class NimBLEServer {
    std::map<std::string,std::unique_ptr<NimBLEService>> services_;
public:
    NimBLEServerCallbacks *callbacks=nullptr;
    void setCallbacks(NimBLEServerCallbacks *value,bool) { callbacks=value; }
    void advertiseOnDisconnect(bool) {}
    void disconnect(uint16_t) {}
    NimBLEService *createService(const char *uuid) {
        auto service=std::make_unique<NimBLEService>(); auto *result=service.get();
        services_[uuid]=std::move(service); return result;
    }
    NimBLEService *getService(const char *uuid) { return services_.at(uuid).get(); }
    bool start() { return true; }
};
class NimBLEAdvertisementData {
public:
    bool setFlags(uint8_t) { return true; }
    bool addServiceUUID(const char *) { return true; }
    bool setName(const char *) { return true; }
};
class NimBLEAdvertising {
    bool advertising_=false;
public:
    void enableScanResponse(bool) {}
    bool setAdvertisementData(const NimBLEAdvertisementData &) { return true; }
    bool setScanResponseData(const NimBLEAdvertisementData &) { return true; }
    bool start() { advertising_=true; return true; }
    bool isAdvertising() { return advertising_; }
};
class NimBLEDevice {
    inline static bool initialized_=false;
    inline static std::unique_ptr<NimBLEServer> server_;
    inline static NimBLEAdvertising advertising_;
public:
    static bool init(const char *) { initialized_=true; return true; }
    static bool setPower(int) { return true; }
    static NimBLEServer *createServer() { server_=std::make_unique<NimBLEServer>(); return server_.get(); }
    static NimBLEAdvertising *getAdvertising() { return &advertising_; }
    static bool isInitialized() { return initialized_; }
    static void deinit(bool) { initialized_=false; server_.reset(); }
};
#endif
