/* [新增] 真實主迴圈、量測、BBP、設定、OTA；只替代 MCU、Flash、BLE stack。 */
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include <NimBLEDevice.h>
#include <esp_ota_ops.h>
#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_bbp.h"
uint64_t test_clock_us=100000U;
void (*test_interrupts[32])(void)={};
TestGPIO GPIO={};
bool test_notify_ok=true;
std::map<std::string,std::vector<uint8_t>> test_nvs;

bool test_flash_fail=false;
void brd_io_begin() {}
void brd_battery_begin() {}
void brd_battery_update() {}
bool brd_battery_measurement_allowed() { return true; }
bool brd_battery_restart_required() { return false; }
uint8_t brd_battery_get_percent() { return 80U; }
bool brd_io_is_charging() { return false; }
void brd_oled_begin() {}
static unsigned oled_updates = 0U;
void brd_oled_update() { oled_updates++; }
#include "../BRD_BPX.ino"
int main() {
    brd_settings_begin(); brd_measurement_begin(); assert(brd_bbp_begin());
    auto *server=NimBLEDevice::getServer(); NimBLEConnInfo peer(7U);
    server->callbacks->onConnect(server,peer);
    auto *bbp=server->getService(BBP_SERVICE_UUID)->getCharacteristic(BBP_CHARACTERISTIC_UUID);
    auto *settings=server->getService(SETTINGS_SERVICE_UUID)->getCharacteristic(SETTINGS_RESPONSE_UUID);
    auto *ota=server->getService(OTA_SERVICE_UUID)->getCharacteristic(OTA_STATUS_UUID);
    for(auto *c:{bbp,settings,ota}) { c->getCallbacks()->onSubscribe(c,peer,1U); }
    brd_bbp_update(false); test_clock_us+=200000U;
    auto *control=server->getService(OTA_SERVICE_UUID)->getCharacteristic(OTA_CONTROL_UUID);
    const uint8_t bytes[2]={1U,7U}; control->setValue(bytes,sizeof(bytes)); control->getCallbacks()->onWrite(control,peer);
    unsigned before=test_notify_attempts; loop();
    assert(test_notify_attempts-before==1U);
    for(unsigned i=0;i<20U;i++) { before=test_notify_attempts; loop(); assert(test_notify_attempts-before<=1U); }
    puts("PASS: BBP/settings/OTA share one Notify attempt per real main-loop cycle.");

    /* [新增] BBP 等待重試的同時，真實量測／主迴圈持續接收 30000 RPM。 */
    const uint8_t dump = 0x74U;
    bbp->setValue(&dump, 1U); bbp->getCallbacks()->onWrite(bbp, peer);
    test_notify_ok = false;
    GPIO.in.val |= 1U << LOAD_IR_GPIO;
    assert(test_interrupts[LOAD_IR_GPIO]); test_interrupts[LOAD_IR_GPIO]();
    for (unsigned i = 0U; i < 52U; i++) { loop(); }
    GPIO.in.val |= 1U << RPM_IR_GPIO;
    assert(test_interrupts[RPM_IR_GPIO]); test_interrupts[RPM_IR_GPIO]();
    uint64_t edge_us = test_clock_us;
    const unsigned display_before = oled_updates;
    const size_t packets_before_recovery = bbp->sent.size();
    for (unsigned i = 0U; i < 1500U; i++) {
        edge_us += 2000U; test_clock_us = edge_us; test_interrupts[RPM_IR_GPIO]();
        before = test_notify_attempts; loop();
        assert(test_notify_attempts - before <= 1U);
        assert(brd_measurement_get_display().value == 30000U);
    }
    assert(oled_updates - display_before == 1500U);
    assert(server->disconnect_requests.empty());
    auto diagnostics = brd_measurement_get_diagnostics();
    assert(diagnostics.rpm_events_dropped == 0U && diagnostics.rpm_capture_disabled == 0U);
    assert(diagnostics.invalid_measurements == 0U);
    assert(brd_bbp_get_diagnostics().notify_failures > 0U);
    edge_us += 20000U; test_clock_us = edge_us; test_interrupts[RPM_IR_GPIO](); loop();
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 30000U);
    test_notify_ok = true;
    for (unsigned i = 0U; i < 500U; i++) {
        before = test_notify_attempts; loop(); assert(test_notify_attempts - before <= 1U);
    }
    /* 第一份 dump 在新量測開始前凍結，不能因新結果發布而混頁。 */
    /* [V1.4 LOAD Notify 修改] 復原後允許在兩份 12 頁結果之外附帶 LOAD A0。
       驗證結果頁時只索引實際 B0/B6 等曲線頁，不假設最後一頁一定是 B6+6。 */
    assert(bbp->sent.size() >= packets_before_recovery + 24U);
    std::vector<size_t> metadata_indices;
    for (size_t i = packets_before_recovery; i < bbp->sent.size(); i++) {
        if (bbp->sent[i][0] == 0xB6U) { metadata_indices.push_back(i); }
    }
    assert(metadata_indices.size() >= 2U);
    const auto &old_metadata = bbp->sent.at(metadata_indices.front());
    assert(old_metadata[7] == 0U && old_metadata[9] == 0U);
    const auto &new_metadata = bbp->sent.at(metadata_indices.back());
    assert(new_metadata[7] == 0x30U && new_metadata[8] == 0x75U); // 30000，little endian。
    assert(new_metadata[9] == 1U && new_metadata[10] == 0U && new_metadata[11] == 1U);
    assert(server->disconnect_requests.empty());
    puts("PASS: three seconds of failed Notify preserve exact 30000 RPM, OLED service, MAX/history and immutable dump snapshots.");
}
