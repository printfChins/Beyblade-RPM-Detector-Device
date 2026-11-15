/* [V1.4.4 新增] BLE 更名：驗證 NVS 寫入、忙碌拒絕、失敗保留及開機恢復。 */
#include <Arduino.h>
#include <Preferences.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "brd_config.h"
#include "brd_ble_identity.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
std::map<std::string, std::vector<uint8_t>> test_nvs;
unsigned test_flash_writes = 0U;
bool test_flash_fail = false;
static bool safe = true, paused = false, battery_ok = true;
bool brd_measurement_modifications_allowed() { return safe && !paused; }
bool brd_battery_measurement_allowed() { return battery_ok; }
bool brd_measurement_pause_for_settings() { if (!safe || paused) { return false; } paused = true; return true; }
void brd_measurement_resume_after_settings(bool) { paused = false; }
static brd_ble_name_status_t write_name(const char *name) {
    return brd_ble_identity_request_name(reinterpret_cast<const uint8_t *>(name), strlen(name));
}
int main() {
    brd_ble_identity_begin();
    assert(strcmp(brd_ble_identity_active_name(), "BEYBLADE_TOOL01") == 0);
    assert(write_name("space invalid") == BRD_BLE_NAME_INVALID);
    assert(write_name("abcdefghijklmnopqrstuvwxyz") == BRD_BLE_NAME_INVALID);
    safe = false;
    assert(write_name("NEW_NAME") == BRD_BLE_NAME_BUSY);
    safe = true;
    assert(write_name("BEYBLADE_TOOL02") == BRD_BLE_NAME_PENDING);
    assert(write_name("ANOTHER_NAME") == BRD_BLE_NAME_BUSY);
    assert(strcmp(brd_ble_identity_active_name(), "BEYBLADE_TOOL01") == 0);
    safe = false; brd_ble_identity_update();
    assert(test_flash_writes == 0U);
    safe = true; test_flash_fail = true; brd_ble_identity_update();
    assert(brd_ble_identity_status() == BRD_BLE_NAME_FLASH_ERROR);
    assert(strcmp(brd_ble_identity_active_name(), "BEYBLADE_TOOL01") == 0);
    test_flash_fail = false;
    assert(write_name("BEYBLADE_TOOL02") == BRD_BLE_NAME_PENDING);
    battery_ok = false; brd_ble_identity_update();
    assert(test_flash_writes == 1U);
    battery_ok = true; brd_ble_identity_update();
    assert(brd_ble_identity_status() == BRD_BLE_NAME_SAVED);
    assert(strcmp(brd_ble_identity_active_name(), "BEYBLADE_TOOL02") == 0);
    assert(brd_ble_identity_needs_advertising_refresh());
    brd_ble_identity_clear_advertising_refresh();
    assert(!brd_ble_identity_needs_advertising_refresh());
    brd_ble_identity_begin();
    assert(strcmp(brd_ble_identity_active_name(), "BEYBLADE_TOOL02") == 0);
    assert(!brd_ble_identity_needs_advertising_refresh());
    assert(write_name("BEYBLADE_TOOL02") == BRD_BLE_NAME_PENDING);
    const unsigned writes = test_flash_writes;
    brd_ble_identity_update();
    assert(test_flash_writes == writes);
    assert(brd_ble_identity_status() == BRD_BLE_NAME_SAVED);
    puts("PASS: default/name validation, safe NVS write, failure handling and reboot recovery.");
}
