/* [新增] 真正 BLE 設定排程模組的封包、BUSY 與連線世代行為測試。 */
#include <cassert>
#include <cstdio>
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include "brd_config.h"
#include "brd_settings.h"
#include "brd_settings_ble.h"
uint64_t test_clock_us=1000000U;
void (*test_interrupts[32])(void)={};
bool test_notify_ok=true;
static bool allowed=true;
static brd_settings::Manager model({0U,1U,30U,2000U},60000U);
static bool completed=false;
bool brd_measurement_modifications_allowed() { return allowed; }
const brd_settings::Manager &brd_settings_model() { return model; }
void brd_settings_discard_draft() { (void)model.discard_staged(); }
brd_settings::Status brd_settings_process_request(const uint8_t *data,size_t size,uint8_t *out) {
    return brd_settings::handle_request(model,data,size,allowed,out);
}
bool brd_settings_take_completion(uint8_t *out) {
    if(!completed) { return false; }
    completed=false;
    brd_settings::make_response(model,brd_settings::CMD_SAVE,model.save_sequence(),0U,
                               model.last_save_status(),out);
    return true;
}
static void step() { test_clock_us+=11000U; brd_settings_ble_update(); }
static void reconnect_during_dequeue() {
    brd_settings_ble_disconnected(7U);
    brd_settings_ble_connected(7U);
}
static NimBLECharacteristic *race_rx = nullptr;
static void reconnect_and_save_during_dequeue() {
    reconnect_during_dequeue();
    NimBLEConnInfo peer(7U);
    const uint8_t bytes[8] = {0xC1U,1U,brd_settings::CMD_SAVE,12U,0U,0U,0U,0U};
    race_rx->setValue(bytes,sizeof(bytes));
    race_rx->getCallbacks()->onWrite(race_rx,peer);
}
int main() {
    NimBLEServer server;
    assert(brd_settings_ble_create(&server));
    auto *service=server.getService(SETTINGS_SERVICE_UUID);
    auto *rx=service->getCharacteristic(SETTINGS_REQUEST_UUID);
    auto *tx=service->getCharacteristic(SETTINGS_RESPONSE_UUID);
    NimBLEConnInfo peer(7U);
    brd_settings_ble_connected(7U);
    tx->getCallbacks()->onSubscribe(tx,peer,1U); step();
    auto write=[&](uint8_t op,uint8_t seq,uint8_t param=0U,uint8_t value=0U) {
        uint8_t bytes[8]={0xC1U,1U,op,seq,param,value,0U,0U};
        rx->setValue(bytes,sizeof(bytes)); rx->getCallbacks()->onWrite(rx,peer);
    };
    allowed=false;
    write(brd_settings::CMD_SET,1U,brd_settings::PARAM_MODE,1U); step();
    assert(tx->getValue()[4]==brd_settings::BUSY && model.staged().mode==0U);
    write(brd_settings::CMD_GET,2U); step();
    assert(tx->getValue()[4]==brd_settings::OK && tx->getValue()[3]==2U);
    allowed=true; write(brd_settings::CMD_SET,3U,brd_settings::PARAM_MODE,1U);
    allowed=false; step(); // callback 入列後量測開始，主 loop 仍須拒絕。
    assert(tx->getValue()[4]==brd_settings::BUSY && model.staged().mode==0U);
    allowed=true;
    // 即使持續訂閱且 Notify 一直失敗，超時後 GET 仍必須可以處理。
    test_notify_ok=false; write(brd_settings::CMD_GET,4U); step();
    write(brd_settings::CMD_GET,40U);
    test_clock_us += BBP_NOTIFY_TIMEOUT_MS * 1000U;
    step(); step();
    assert(tx->getValue()[3]==40U);
    test_notify_ok=true; step();
    // Notify 失敗後改用 READ，不可讓排程卡死。
    test_notify_ok=false; write(brd_settings::CMD_GET,4U); step();
    tx->getCallbacks()->onSubscribe(tx,peer,0U);
    write(brd_settings::CMD_SET,5U,brd_settings::PARAM_MODE,1U); step();
    assert(model.staged().mode==1U && tx->getValue()[3]==5U);
    test_notify_ok=true; tx->getCallbacks()->onSubscribe(tx,peer,1U); step();
    write(brd_settings::CMD_SAVE,6U); step();
    assert(model.pending() && tx->getValue()[4]==brd_settings::PENDING);
    model.complete_save(true); completed=true; step();
    assert(tx->getValue()[4]==brd_settings::SAVED && tx->getValue()[3]==6U);
    write(brd_settings::CMD_SET,7U,brd_settings::PARAM_MODE,0U);
    brd_settings_ble_disconnected(7U); brd_settings_ble_connected(7U); step(); step();
    assert(model.staged().mode==1U); // 舊連線排入的 SET 不可套到新連線。
    write(brd_settings::CMD_SET,8U,brd_settings::PARAM_MODE,0U); step();
    assert(model.staged().mode==0U);
    brd_settings_ble_disconnected(7U); step();
    assert(model.staged().mode==1U); // 未 SAVE 草稿於斷線丟棄。
    brd_settings_ble_connected(7U); step();
    write(brd_settings::CMD_SET,9U,brd_settings::PARAM_MODE,0U); step();
    write(brd_settings::CMD_SAVE,10U);
    test_queue_receive_hook = reconnect_during_dequeue;
    step(); step(); // 讀取 link snapshot 後才斷線：舊 SAVE 不可接受。
    assert(!model.pending() && model.staged().mode==1U);
    write(brd_settings::CMD_SET,11U,brd_settings::PARAM_MODE,0U); step();
    race_rx = rx;
    test_queue_receive_hook = reconnect_and_save_during_dequeue;
    step(); // 新連線立即 SAVE，也不可繼承斷線前的草稿。
    assert(!model.pending() && model.staged().mode==1U);
    puts("PASS: settings GATT packets, BUSY/GET, second gate, READ fallback and connection generations.");
}
