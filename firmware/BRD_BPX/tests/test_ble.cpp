/* [新增] 真正 BLE 設定排程模組的封包、BUSY 與連線世代行為測試。 */
#include <cassert>
#include <cstdio>
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include "brd_config.h"
#include "brd_settings.h"
#include "brd_settings_ble.h"
#include "brd_ble_identity.h"
#include <map>
#include <string>
uint64_t test_clock_us=1000000U;
void (*test_interrupts[32])(void)={};
bool test_notify_ok=true;
/* [V1.4.4 新增] 名稱 NVS host stub 與安全寫入窗口。 */
std::map<std::string, std::vector<uint8_t>> test_nvs;
unsigned test_flash_writes=0U;
bool test_flash_fail=false;
bool brd_battery_measurement_allowed() { return true; }
bool brd_measurement_pause_for_settings() { return true; }
void brd_measurement_resume_after_settings(bool) {}
static bool allowed=true;
static brd_settings::Manager model({0U,1U,30U,2000U,BATTERY_CAPACITY_200_MAH, OLED_MAX_HOLD_MS},60000U);
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
    brd_ble_identity_begin();
    NimBLEServer server;
    assert(brd_settings_ble_create(&server));
    auto *service=server.getService(SETTINGS_SERVICE_UUID);
    auto *rx=service->getCharacteristic(SETTINGS_REQUEST_UUID);
    auto *tx=service->getCharacteristic(SETTINGS_RESPONSE_UUID);
    /* [新增] 讀寫獨立 OLED display-page GATT，不改舊設定回覆尺寸。 */
    auto *page=service->getCharacteristic(SETTINGS_DISPLAY_PAGE_UUID);
    assert(page!=nullptr);
    NimBLEConnInfo peer(7U);
    brd_settings_ble_connected(7U);
    assert(brd_settings_ble_display_page()==BRD_DISPLAY_PAGE_MAIN);
    auto page_write=[&](uint8_t value) {
        page->setValue(&value,1U);
        page->getCallbacks()->onWrite(page,peer);
        page->getCallbacks()->onRead(page,peer);
    };
    page_write(1U);
    assert(brd_settings_ble_display_page()==BRD_DISPLAY_PAGE_IR_TEST);
    assert(page->getValue().size()==1U && page->getValue()[0]==1U);
    page_write(2U); // [新增] 非法值必須忽略，READ 回前次合法值。
    assert(brd_settings_ble_display_page()==BRD_DISPLAY_PAGE_IR_TEST);
    assert(page->getValue()[0]==1U);
    page_write(0U);
    assert(brd_settings_ble_display_page()==BRD_DISPLAY_PAGE_MAIN);
    page_write(1U);
    brd_settings_ble_disconnected(7U);
    assert(brd_settings_ble_display_page()==BRD_DISPLAY_PAGE_MAIN);
    brd_settings_ble_connected(7U);
    page->getCallbacks()->onRead(page,peer);
    assert(page->getValue()[0]==0U);

    tx->getCallbacks()->onSubscribe(tx,peer,1U); step();
    auto write=[&](uint8_t op,uint8_t seq,uint8_t param=0U,uint32_t value=0U) {
        /* [V1.4.4 修改] 依原 uint24 指令格式送出高位元組。 */
        uint8_t bytes[8]={0xC1U,1U,op,seq,param,static_cast<uint8_t>(value),
                          static_cast<uint8_t>(value >> 8U),static_cast<uint8_t>(value >> 16U)};
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
    /* [V1.4.3 新增] 真正 GATT 接受 300mAh uint24 設定，且回覆仍為 20 bytes。 */
    const uint8_t capacity_request[8] = {0xC1U, 1U, brd_settings::CMD_SET, 13U,
                                         brd_settings::PARAM_BATTERY_CAPACITY_MAH, 0x2CU, 0x01U, 0x00U};
    rx->setValue(capacity_request, sizeof(capacity_request));
    rx->getCallbacks()->onWrite(rx, peer);
    step();
    assert(model.staged().battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    assert(model.active().battery_capacity_mah == BATTERY_CAPACITY_200_MAH);
    assert(tx->getValue().size() == brd_settings::RESPONSE_SIZE &&
           (tx->getValue()[6] & 0x18U) == 0x10U);
    write(brd_settings::CMD_SAVE, 14U); step();
    assert(model.pending());
    model.complete_save(true); completed = true; step();
    assert(model.active().battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    assert((tx->getValue()[6] & 0x18U) == 0x18U);
    /* [V1.4.4 新增] 版本、250mAh、HOLD 及裝置名稱均以獨立 GATT 驗證。 */
    auto *fw=service->getCharacteristic(SETTINGS_FW_VERSION_UUID);
    auto *capacity=service->getCharacteristic(SETTINGS_BATTERY_CAPACITY_UUID);
    auto *hold=service->getCharacteristic(SETTINGS_HOLD_DURATION_UUID);
    auto *name=service->getCharacteristic(SETTINGS_DEVICE_NAME_UUID);
    auto *name_status=service->getCharacteristic(SETTINGS_DEVICE_NAME_STATUS_UUID);
    assert(fw && capacity && hold && name && name_status);
    fw->getCallbacks()->onRead(fw,peer);
    auto fwbytes=fw->getValue();
    /* [V1.5 修改] 驗證 BLE 版本 READ 回傳值與當前韌體一致。 */
    assert(std::string(fwbytes.begin(),fwbytes.end())=="V1.5");
    capacity->getCallbacks()->onRead(capacity,peer);
    assert(capacity->getValue().size()==4U && capacity->getValue()[0]==0x2CU);
    const uint8_t hold_request[8]={0xC1U,1U,brd_settings::CMD_SET,15U,
                                   brd_settings::PARAM_HOLD_MS,0xA0U,0x0FU,0U};
    rx->setValue(hold_request,sizeof(hold_request)); rx->getCallbacks()->onWrite(rx,peer); step();
    hold->getCallbacks()->onRead(hold,peer);
    assert(hold->getValue().size()==4U && hold->getValue()[2]==0xA0U && hold->getValue()[3]==0x0FU);
    const uint8_t capacity_250_request[8]={0xC1U,1U,brd_settings::CMD_SET,16U,
        brd_settings::PARAM_BATTERY_CAPACITY_MAH,0xFAU,0U,0U};
    rx->setValue(capacity_250_request,sizeof(capacity_250_request));
    rx->getCallbacks()->onWrite(rx,peer); step();
    capacity->getCallbacks()->onRead(capacity,peer);
    assert(capacity->getValue()[2]==0xFAU && capacity->getValue()[3]==0U);
    write(brd_settings::CMD_SAVE,17U); step();
    assert(model.pending()); model.complete_save(true); completed=true; step();
    assert(model.active().hold_ms==4000U && model.active().battery_capacity_mah==250U);
    capacity->getCallbacks()->onRead(capacity,peer);
    assert(capacity->getValue()[0]==0xFAU && capacity->getValue()[2]==0xFAU);
    name->getCallbacks()->onRead(name,peer);
    auto old_name=name->getValue();
    assert(std::string(old_name.begin(),old_name.end())=="BEYBLADE_TOOL01");
    const char renamed[]="BEYBLADE_TOOL77";
    name->setValue(reinterpret_cast<const uint8_t *>(renamed),sizeof(renamed)-1U);
    name->getCallbacks()->onWrite(name,peer);
    name_status->getCallbacks()->onRead(name_status,peer);
    assert(name_status->getValue()[0]==BRD_BLE_NAME_PENDING);
    brd_ble_identity_update();
    name_status->getCallbacks()->onRead(name_status,peer);
    assert(name_status->getValue()[0]==BRD_BLE_NAME_SAVED);
    name->getCallbacks()->onRead(name,peer);
    auto new_name=name->getValue();
    assert(std::string(new_name.begin(),new_name.end())==renamed);

    /* [V1.4.4 新增] 大容量 SET 與 SAVE 前後都以實際 GATT READ 檢查 LE 格式。 */
    auto assert_capacity_read = [&](uint8_t active_low, uint8_t active_high,
                                    uint8_t staged_low, uint8_t staged_high) {
        capacity->getCallbacks()->onRead(capacity, peer);
        const auto bytes = capacity->getValue();
        assert(bytes.size() == 4U && bytes[0] == active_low && bytes[1] == active_high &&
               bytes[2] == staged_low && bytes[3] == staged_high);
    };
    write(brd_settings::CMD_SET, 18U, brd_settings::PARAM_BATTERY_CAPACITY_MAH, 500U); step();
    assert(tx->getValue()[4] == brd_settings::OK);
    assert_capacity_read(0xFAU, 0x00U, 0xF4U, 0x01U);
    write(brd_settings::CMD_SAVE, 19U); step();
    assert(model.pending()); model.complete_save(true); completed = true; step();
    assert(tx->getValue()[4] == brd_settings::SAVED);
    assert_capacity_read(0xF4U, 0x01U, 0xF4U, 0x01U);
    write(brd_settings::CMD_SET, 20U, brd_settings::PARAM_BATTERY_CAPACITY_MAH, 1000U); step();
    assert(tx->getValue()[4] == brd_settings::OK);
    assert_capacity_read(0xF4U, 0x01U, 0xE8U, 0x03U);
    write(brd_settings::CMD_SAVE, 21U); step();
    assert(model.pending()); model.complete_save(true); completed = true; step();
    assert(tx->getValue()[4] == brd_settings::SAVED);
    assert_capacity_read(0xE8U, 0x03U, 0xE8U, 0x03U);
    puts("PASS: settings GATT, fw/read, capacity SET/SAVE/READ, HOLD, name save and link generations.");
}
