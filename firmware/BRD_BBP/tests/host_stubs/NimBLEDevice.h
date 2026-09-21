/* 檔案位置: BRD_BBP/tests/host_stubs/NimBLEDevice.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#define BLE_HS_CONN_HANDLE_NONE 0xffff
namespace NIMBLE_PROPERTY { enum {NOTIFY=1,WRITE=2,WRITE_NR=4}; }
class NimBLEServer; class NimBLECharacteristic;
class NimBLEConnInfo{uint16_t h_;public:explicit NimBLEConnInfo(uint16_t h):h_(h){}uint16_t getConnHandle()const{return h_;}};
class NimBLEServerCallbacks{public:virtual~NimBLEServerCallbacks()=default;virtual void onConnect(NimBLEServer*,NimBLEConnInfo&){}virtual void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int){}};
class NimBLECharacteristicCallbacks{public:virtual~NimBLECharacteristicCallbacks()=default;virtual void onSubscribe(NimBLECharacteristic*,NimBLEConnInfo&,uint16_t){}virtual void onWrite(NimBLECharacteristic*,NimBLEConnInfo&){} };
namespace host_ble { inline std::vector<std::vector<uint8_t>> notifications; inline std::vector<bool> notify_results; inline unsigned notify_index=0; inline unsigned init_calls=0; inline bool initialized=false; inline uint16_t disconnected=BLE_HS_CONN_HANDLE_NONE; }
class NimBLECharacteristic{std::string value_;NimBLECharacteristicCallbacks* cb_=nullptr;public:void setCallbacks(NimBLECharacteristicCallbacks*c){cb_=c;}std::string getValue()const{return value_;}void hostWrite(std::string v,uint16_t h){value_=v;NimBLEConnInfo i(h);cb_->onWrite(this,i);}void hostSubscribe(uint16_t h,uint16_t v){NimBLEConnInfo i(h);cb_->onSubscribe(this,i,v);}bool notify(const uint8_t*p,size_t n,uint16_t){host_ble::notifications.emplace_back(p,p+n);if(host_ble::notify_index<host_ble::notify_results.size())return host_ble::notify_results[host_ble::notify_index++];return true;}};
class NimBLEService{public:NimBLECharacteristic c;NimBLECharacteristic*createCharacteristic(const char*,int,size_t){return &c;}};
class NimBLEServer{NimBLEServerCallbacks*cb_=nullptr;public:NimBLEService s;void setCallbacks(NimBLEServerCallbacks*c,bool){cb_=c;}void advertiseOnDisconnect(bool){}NimBLEService*createService(const char*){return &s;}bool start(){return true;}void disconnect(uint16_t h){host_ble::disconnected=h;if(cb_){NimBLEConnInfo i(h);cb_->onDisconnect(this,i,0);}}void hostConnect(uint16_t h){NimBLEConnInfo i(h);cb_->onConnect(this,i);}};
class NimBLEAdvertisementData{public:bool setFlags(uint8_t){return true;}bool addServiceUUID(const char*){return true;}bool setName(const char*){return true;}};
class NimBLEAdvertising{bool active_=false;public:void enableScanResponse(bool){}bool setAdvertisementData(const NimBLEAdvertisementData&){return true;}bool setScanResponseData(const NimBLEAdvertisementData&){return true;}bool start(){active_=true;return true;}bool isAdvertising()const{return active_;}};
class NimBLEDevice{public:inline static NimBLEServer server;inline static NimBLEAdvertising advertising;static bool init(const char*){++host_ble::init_calls;host_ble::initialized=true;return true;}static bool setPower(int){return true;}static NimBLEServer*createServer(){return &server;}static NimBLEAdvertising*getAdvertising(){return &advertising;}static bool isInitialized(){return host_ble::initialized;}static void deinit(bool){host_ble::initialized=false;}};
