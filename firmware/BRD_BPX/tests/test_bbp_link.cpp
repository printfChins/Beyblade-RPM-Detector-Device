/* [新增] 真實 BBP 傳輸狀態機；只替代 MCU / NVS / BLE stack 邊界。 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "brd_bbp.h"
#include "brd_config.h"
#include "brd_measurement.h"
/* [V1.2.1] V1.2 無 scheduler，不移植 V1.3 RPM 排程。 */

uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
bool test_notify_ok = true;
std::map<std::string, std::vector<uint8_t>> test_nvs;
bool test_flash_fail = false;
unsigned test_flash_writes = 0U;
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

static void step(uint32_t ms = 10U) {
    test_clock_us += static_cast<uint64_t>(ms) * 1000U;
    brd_bbp_update(false);
}
static void command(NimBLECharacteristic *bbp, NimBLEConnInfo &peer, uint8_t value) {
    bbp->setValue(&value, 1U);
    bbp->getCallbacks()->onWrite(bbp, peer);
}
static void assert_page(const NimBLECharacteristic *bbp, size_t index, uint8_t type) {
    assert(bbp->sent.at(index).size() == 17U);
    assert(bbp->sent.at(index)[0] == type);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const unsigned scenario = static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10));
    assert(brd_bbp_begin());
    auto *server = NimBLEDevice::getServer();
    NimBLEConnInfo peer(7U);
    server->callbacks->onConnect(server, peer);
    auto *bbp = server->getService(BBP_SERVICE_UUID)->getCharacteristic(BBP_CHARACTERISTIC_UUID);
    bbp->getCallbacks()->onSubscribe(bbp, peer, 1U);
    step(0U); step(100U);
    assert(bbp->sent.size() == 1U);
    assert_page(bbp, 0U, 0xA0U);
    assert(server->disconnect_requests.empty());
    if (scenario == 1U) {
        /* 一包 A0、第一頁 B0 已成功，下一頁失敗超過舊版 1.5 秒。 */
        command(bbp, peer, 0x74U); step();
        assert_page(bbp, 1U, 0xB0U);
        test_notify_ok = false;
        for (unsigned i = 0U; i < 220U; i++) { step(); }
        assert(server->disconnect_requests.empty());
        assert(brd_bbp_is_connected());
        test_notify_ok = true;
        for (unsigned i = 0U; i < 50U; i++) { step(); }
        const uint8_t pages[] = {0xB0U,0xB1U,0xB2U,0xB3U,0xB4U,0xB5U,0xB6U,0xB7U,0x70U,0x71U,0x72U,0x73U};
        assert(bbp->sent.size() == 13U);
        for (size_t i = 0U; i < sizeof(pages); i++) { assert_page(bbp, i + 1U, pages[i]); }
        puts("PASS: Notify stall after initial success keeps link and resumes the same 12-page snapshot.");
    } else if (scenario == 2U) {
        /* 第一包成功後，手機突發輪詢超過 32 筆容量。已接受命令仍須完成。 */
        for (unsigned i = 0U; i < 40U; i++) { command(bbp, peer, 0x51U); }
        assert(brd_bbp_get_diagnostics().command_queue_overflows == 8U);
        for (unsigned i = 0U; i < 50U; i++) { step(); }
        assert(server->disconnect_requests.empty());
        assert(bbp->sent.size() == 33U);
        for (size_t i = 1U; i < bbp->sent.size(); i++) { assert_page(bbp, i, 0xA0U); }
        command(bbp, peer, 0x51U); step();
        assert(bbp->sent.size() == 34U);
        puts("PASS: bounded command overflow preserves the connection and all 32 accepted requests.");
    } else if (scenario == 3U) {
        command(bbp, peer, 0x74U); test_notify_ok = false;
        unsigned before = test_notify_attempts;
        for (unsigned i = 0U; i < 1000U; i++) { step(1U); }
        assert(test_notify_attempts - before <= 11U);
        assert(test_notify_attempts - before >= 1U);
        assert(server->disconnect_requests.empty());
        puts("PASS: stalled BLE retries stay bounded while the main loop continues.");
    } else if (scenario == 4U) {
        /* [V1.2.1] V1.2 主迴圈沒有 background_ready gate；只測兩秒主迴圈遲滯不會導致斷線。 */
        command(bbp, peer, 0x74U);
        test_clock_us += 2000000U;
        test_notify_ok = false;
        const unsigned before = test_notify_attempts;
        step();
        assert(test_notify_attempts == before + 1U);
        assert(server->disconnect_requests.empty());
        test_notify_ok = true;
        for (unsigned i = 0; i < 20U; i++) { step(); }
        assert(server->disconnect_requests.empty());
        puts("PASS: 2-second service delay cannot cause a forced BLE disconnect.");
    } else if (scenario == 5U) {
        command(bbp, peer, 0x74U); step();
        test_notify_ok = false; step();
        NimBLEConnInfo other_peer(8U);
        server->callbacks->onDisconnect(server, other_peer, 0x216);
        assert(brd_bbp_is_connected());
        assert(brd_bbp_get_diagnostics().disconnect_events == 0U);
        server->callbacks->onDisconnect(server, peer, 0x213);
        assert(!brd_bbp_is_connected());
        const auto diagnostics = brd_bbp_get_diagnostics();
        assert(diagnostics.disconnect_events == 1U && diagnostics.last_disconnect_reason == 0x213);
        step();
        server->callbacks->onConnect(server, peer);
        bbp->getCallbacks()->onSubscribe(bbp, peer, 1U);
        test_notify_ok = true; step(); step(100U);
        assert(bbp->sent.size() == 3U);
        assert_page(bbp, 2U, 0xA0U);
        puts("PASS: real disconnect/reconnect with reused handle discards the old partial dump.");
    } else if (scenario == 6U) {
        command(bbp, peer, 0x74U); step();
        bbp->getCallbacks()->onSubscribe(bbp, peer, 0U); step();
        const size_t before = bbp->sent.size();
        for (unsigned i = 0U; i < 200U; i++) { step(); }
        assert(bbp->sent.size() == before);
        assert(server->disconnect_requests.empty());
        bbp->getCallbacks()->onSubscribe(bbp, peer, 1U); step(); step(100U);
        assert(bbp->sent.size() == before + 1U);
        assert_page(bbp, before, 0xA0U);
        puts("PASS: unsubscribe stops retries without disconnect; resubscribe starts a fresh header.");
    } else { assert(false); }
}
