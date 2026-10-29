/* [V1.2 新增] 量測完成與 OTA BEGIN 同輪、發布延遲與 NVS 保存的真實 BBP 邊界。 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "brd_bbp.h"
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_bbp_protocol.h"
#include <map>
#include <string>
#include <vector>
#include <assert.h>
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
bool test_notify_ok = true;
std::map<std::string, std::vector<uint8_t>> test_nvs;
unsigned test_flash_writes = 0U;
bool test_flash_fail = false;
bool brd_battery_measurement_allowed() { return true; }
uint8_t brd_battery_get_percent() { return 80U; }
bool brd_settings_bluetooth_allowed(bool) { return true; }
bool brd_io_is_charging() { return false; }
bool brd_measurement_modifications_allowed() { return true; }
bool brd_measurement_pause_for_settings() { return true; }
void brd_measurement_resume_after_settings(bool) {}
brd_display_t brd_measurement_get_display() { return {}; }
bool brd_settings_ble_create(NimBLEServer *) { return true; }
void brd_settings_ble_connected(uint16_t) {}
void brd_settings_ble_disconnected(uint16_t) {}
void brd_settings_ble_stop() {}
bool brd_ota_create(NimBLEServer *) { return true; }
void brd_ota_connected(uint16_t) {}
void brd_ota_disconnected(uint16_t) {}
void brd_ota_stop() {}
int main() {
    assert(brd_bbp_begin()); assert(brd_bbp_ota_ready());
    brd_bbp_capture_period(4000U, micros(), 1U);
    brd_bbp_capture_launch(micros() + 10000U); brd_bbp_capture_finish();
    /* [新增] 正是同一輪 finish -> BEGIN 的位置；dirty 尚未建立也必須拒絕。 */
    assert(!brd_bbp_ota_ready());
    test_clock_us = 100000U + BBP_PUBLISH_MIN_DELAY_US - 1U;
    brd_bbp_update(true); assert(!brd_bbp_ota_ready());
    test_clock_us++; brd_bbp_update(true);
    assert(!brd_bbp_ota_ready());
    test_clock_us += BBP_RETRY_INTERVAL_MS * 1000U; brd_bbp_update(true);
    assert(brd_bbp_ota_ready());
    const auto &blob = test_nvs.at(std::string(BBP_NVS_NAMESPACE) + "/" + BBP_NVS_KEY);
    brd_bbp::State restored;
    assert(brd_bbp::decode_storage(blob.data(), blob.size(), restored));
    assert(restored.total == 1U && test_flash_writes == 1U);
    puts("PASS: pending last-shot publication and NVS persistence gate OTA start, including 600ms delay.");
}
