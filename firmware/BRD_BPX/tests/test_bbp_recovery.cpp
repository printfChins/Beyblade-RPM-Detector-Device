/* [V1.4.5 新增] 真實 BBP / Session / 名稱模組，驗證壞歷史與改名競態。 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <cassert>
#include <cstdio>
#include <string>
#include "brd_bbp.h"
#include "brd_bbp_protocol.h"
#include "brd_ble_identity.h"
#include "brd_config.h"
#include "brd_measurement.h"

uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
bool test_notify_ok = true;
std::map<std::string, std::vector<uint8_t>> test_nvs;
bool test_flash_fail = false;
unsigned test_flash_writes = 0U;
static void (*modifications_hook)() = nullptr;
/* [新增] 模擬完成後仍處於 HOLD，該窗口不得接受 0x75 清除歷史。 */
static bool measurement_safe = true;
bool brd_battery_measurement_allowed() { return true; }
uint8_t brd_battery_get_percent() { return 80U; }
bool brd_settings_bluetooth_allowed(bool) { return true; }
bool brd_settings_save_pending() { return false; }
bool brd_io_is_charging() { return false; }
bool brd_measurement_capture_active() { return false; }
bool brd_measurement_modifications_allowed() {
    if (modifications_hook) {
        auto hook = modifications_hook; modifications_hook = nullptr; hook();
    }
    return measurement_safe;
}
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
static NimBLEConnInfo peer(7U);
static NimBLEServer *server = nullptr;
static NimBLECharacteristic *bbp = nullptr;
static const std::string key = std::string(BBP_NVS_NAMESPACE) + "/" + BBP_NVS_KEY;
static void start() {
    brd_ble_identity_begin();
    assert(brd_bbp_begin());
    server = NimBLEDevice::getServer();
    bbp = server->getService(BBP_SERVICE_UUID)->getCharacteristic(BBP_CHARACTERISTIC_UUID);
}
static void connect() {
    NimBLEDevice::getAdvertising()->stopForTest();
    server->callbacks->onConnect(server, peer);
    assert(brd_bbp_is_connected());
}
static void disconnect() { server->callbacks->onDisconnect(server, peer, 0x213); }
static void advance(unsigned ms = 1000U, bool storage = true) {
    test_clock_us += static_cast<uint64_t>(ms) * 1000U;
    brd_bbp_update(storage);
}
static void command(uint8_t value) {
    bbp->setValue(&value, 1U);
    bbp->getCallbacks()->onWrite(bbp, peer);
    brd_bbp_update(true);
}
static void seed() {
    brd_bbp::State state = {};
    brd_bbp::Profile profile;
    profile.add_period_us(5000U);
    assert(brd_bbp::append(state, profile, 12000U));
    uint8_t bytes[brd_bbp::STORAGE_SIZE];
    brd_bbp::encode_storage(state, bytes);
    test_nvs[key] = std::vector<uint8_t>(bytes, bytes + sizeof(bytes));
}
static void capture() {
    brd_bbp_capture_reset();
    brd_bbp_capture_period(5000U, micros(), true);
    brd_bbp_capture_launch(micros());
    brd_bbp_capture_finish();
}
static unsigned count_ram() {
    bbp->getCallbacks()->onSubscribe(bbp, peer, 1U);
    advance(200U);
    command(0x51U);
    for (unsigned i = 0; i < 20U; ++i) { advance(10U); }
    for (auto it = bbp->sent.rbegin(); it != bbp->sent.rend(); ++it) {
        if ((*it)[0] == 0xA0U) { return (*it)[9] | (unsigned((*it)[10]) << 8U); }
    }
    assert(false); return 0U;
}
static void request_name() {
    const uint8_t name[] = {'B','R','D','_','N','E','W'};
    assert(brd_ble_identity_request_name(name, sizeof(name)) == BRD_BLE_NAME_PENDING);
    brd_ble_identity_update();
    assert(brd_ble_identity_needs_advertising_refresh());
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    if (scenario.rfind("history-", 0U) == 0U) {
        seed();
        if (scenario == "history-crc" || scenario == "history-write-fail") { test_nvs[key].back() ^= 1U; }
        else if (scenario == "history-length") { test_nvs[key].pop_back(); }
        else if (scenario == "history-version") { test_nvs[key][0] ^= 1U; }
        else if (scenario == "history-read") { test_preferences_read_ok = false; }
        else if (scenario == "history-open") { test_preferences_begin_ok = false; }
        else { assert(false); }
        const auto original = test_nvs[key];
        start(); // 舊版在此失敗：歷史不可解碼不應阻止 BLE 啟動。
        assert(NimBLEDevice::getAdvertising()->isAdvertising());
        assert(brd_bbp_ota_ready());
        capture();
        assert(!brd_bbp_ota_ready());
        advance();
        connect();
        assert(count_ram() == 1U);
        assert(test_nvs[key] == original && test_flash_writes == 0U);
        assert(brd_bbp_prepare_restart());
        test_preferences_read_ok = test_preferences_begin_ok = true;
        if (scenario == "history-write-fail") { test_flash_fail = true; }
        command(0x75U);
        if (test_flash_fail) {
            assert(test_nvs[key] == original);
            assert(!brd_bbp_ota_ready());
            test_flash_fail = false;
            advance();
        }
        brd_bbp::State restored;
        assert(brd_bbp::decode_storage(test_nvs[key].data(), test_nvs[key].size(), restored));
        assert(restored.total == 0U);
        capture(); advance();
        assert(brd_bbp::decode_storage(test_nvs[key].data(), test_nvs[key].size(), restored));
        assert(restored.total == 1U);
    } else if (scenario == "pending-clear") {
        start(); connect(); capture();
        measurement_safe = false;  // 模擬已停止取樣、仍在 HOLD；禁止清歷史。
        command(0x75U);
        advance();
        assert(count_ram() == 1U);
    } else {
        start(); connect(); request_name();
        advance();
        assert(brd_bbp_is_connected() && test_deinit_calls == 0U);
        disconnect();
        advance(1000U, false);
        if (scenario == "rename-pending") {
            assert(!NimBLEDevice::getAdvertising()->isAdvertising());
        } else if (scenario == "rename-before-check") {
            modifications_hook = connect;
        } else if (scenario == "rename-at-stop") {
            test_advertising_stop_hook = connect;
        } else if (scenario == "rename-after-stop") {
            test_set_device_name_hook = connect;
        } else if (scenario == "rename-stop-fail") {
            test_advertising_stop_ok = false;
        } else if (scenario == "rename-set-fail") {
            test_scan_response_ok = false;
        } else if (scenario == "rename-gap-fail") {
            test_device_name_ok = false;
        } else if (scenario == "rename-start-fail") {
            test_advertising_start_ok = false;
        } else if (scenario != "rename-success") { assert(false); }
        advance();
        assert(test_deinit_calls == 0U); // 舊版會重建 BLE，甚至清除已接受的新連線。
        if (scenario == "rename-before-check" || scenario == "rename-at-stop" || scenario == "rename-after-stop") {
            assert(brd_bbp_is_connected());
            assert(server->disconnect_requests.empty());
            assert(count_ram() == 0U); // 新連線仍能使用原 GATT 服務。
            disconnect();
        }
        if (scenario == "rename-stop-fail" || scenario == "rename-set-fail" || scenario == "rename-gap-fail") {
            assert(brd_ble_identity_needs_advertising_refresh());
            assert(!NimBLEDevice::getAdvertising()->isAdvertising());
        }
        test_advertising_stop_ok = test_scan_response_ok = test_device_name_ok = test_advertising_start_ok = true;
        advance();
        assert(!brd_ble_identity_needs_advertising_refresh());
        assert(test_gap_name == "BRD_NEW" && test_scan_name == "BRD_NEW");
        assert(NimBLEDevice::getAdvertising()->isAdvertising());
        assert(NimBLEDevice::getServer() == server && test_deinit_calls == 0U);
        connect();
        assert(count_ram() == 0U);
    }
    printf("PASS: %s\n", scenario.c_str());
}
