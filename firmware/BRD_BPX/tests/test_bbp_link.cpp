/* [新增] 真實 BBP 傳輸狀態機；只替代 MCU / NVS / BLE stack 邊界。 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "brd_bbp.h"
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_scheduler.h"

uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
bool test_notify_ok = true;
std::map<std::string, std::vector<uint8_t>> test_nvs;
bool test_flash_fail = false;
unsigned test_flash_writes = 0U;
static bool background_ready = true;
bool brd_battery_measurement_allowed() { return true; }
/* [V1.4 BLE 30s 新增] 可變 SOC，確認定時 Notify 封包含最新電量而非訂閱時快照。 */
static uint8_t g_test_percent = 80U;
uint8_t brd_battery_get_percent() { return g_test_percent; }
bool brd_settings_bluetooth_allowed(bool) { return true; }
bool brd_io_is_charging() { return false; }
bool brd_measurement_modifications_allowed() { return true; }
bool brd_measurement_pause_for_settings() { return true; }
void brd_measurement_resume_after_settings(bool) {}
/* [V1.4 LOAD Notify 修改] 模擬去抖後的 LOAD 狀態，驗證切換即觸發 A0。 */
static bool g_test_loaded = false;
brd_display_t brd_measurement_get_display() {
    brd_display_t display = {};
    display.loaded = g_test_loaded;
    return display;
}
bool brd_measurement_background_ready() { return background_ready; }
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
    brd_scheduler_begin_cycle();
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
    /* [V1.4 BLE 30s 新增] 測試 uint32_t millis() 環繞仍維持 30 秒週期。 */
    if (scenario == 12U) {
        test_clock_us = (static_cast<uint64_t>(UINT32_MAX) - 150U) * 1000ULL;
    }
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
        command(bbp, peer, 0x74U); background_ready = false;
        const unsigned before = test_notify_attempts;
        for (unsigned i = 0U; i < 200U; i++) { step(); }
        assert(test_notify_attempts == before);
        background_ready = true; test_notify_ok = false; step();
        assert(test_notify_attempts == before + 1U);
        assert(server->disconnect_requests.empty());
        puts("PASS: yielding two seconds to RPM cannot convert the first Notify failure into a disconnect.");
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
    } else if (scenario == 7U) {
        /* [V1.4 LOAD Notify 修改] LOAD 0->1/1->0 立即排程、空閒不額外通知，30 秒仍更新 SOC。 */
        g_test_loaded = true;
        g_test_percent = 79U;
        step(1U);
        assert(bbp->sent.size() == 1U); // 10ms 封包間隔尚未屆滿
        step(9U);
        assert(bbp->sent.size() == 2U);
        assert_page(bbp, 1U, 0xA0U);
        assert(bbp->sent[0U][4U] == 200U);  // 初始 80%
        assert(bbp->sent[1U][4U] == 198U);  // 最新 79%
        assert((bbp->sent[1U][3U] & 0x04U) == 0x04U);
        step(29999U);
        assert(bbp->sent.size() == 2U);
        step(1U);
        assert(bbp->sent.size() == 3U);
        assert_page(bbp, 2U, 0xA0U);
        assert((bbp->sent[2U][3U] & 0x04U) == 0x04U);
        g_test_loaded = false;
        step(10U);
        assert(bbp->sent.size() == 4U);
        assert_page(bbp, 3U, 0xA0U);
        assert((bbp->sent[3U][3U] & 0x04U) == 0U);
        step(100U);
        assert(bbp->sent.size() == 4U);
        puts("PASS: LOAD both edges Notify on change; periodic 30s still runs; no idle spam.");
    } else if (scenario == 8U) {
        /* [V1.4 BLE 30s 新增] 手機 0x51 回覆更新計時基準，不會緊接著重複自動送 A0。 */
        step(20000U);
        assert(bbp->sent.size() == 1U);
        command(bbp, peer, 0x51U);
        step(10U);
        assert(bbp->sent.size() == 2U);
        assert_page(bbp, 1U, 0xA0U);
        step(9999U);
        assert(bbp->sent.size() == 2U);
        step(20000U);
        assert(bbp->sent.size() == 2U);
        step(1U);
        assert(bbp->sent.size() == 3U);
        assert_page(bbp, 2U, 0xA0U);
        puts("PASS: explicit 0x51 replies reset the periodic timer without an extra A0.");
    } else if (scenario == 9U) {
        /* [V1.4 BLE 30s 新增] 30 秒到期不插入 12 頁結果傳輸；傳完只補最新 A0。 */
        command(bbp, peer, 0x74U);
        step();
        assert_page(bbp, 1U, 0xB0U);
        background_ready = false;
        step(35000U);
        assert(bbp->sent.size() == 2U);
        background_ready = true;
        for (unsigned i = 0U; i < 11U; i++) { step(); }
        assert(bbp->sent.size() == 13U);
        for (size_t i = 1U; i < 13U; i++) {
            assert_page(bbp, i, i <= 8U ? static_cast<uint8_t>(0xAFU + i) :
                                      static_cast<uint8_t>(0x70U + i - 9U));
        }
        step();
        assert(bbp->sent.size() == 14U);
        assert_page(bbp, 13U, 0xA0U);
        step();
        assert(bbp->sent.size() == 14U);
        puts("PASS: periodic battery A0 never interrupts the 12-page BBP result dump.");
    } else if (scenario == 10U) {
        /* [V1.4 BLE 30s 新增] 訂閱狀態已移除時，不應累積定期封包。 */
        bbp->getCallbacks()->onSubscribe(bbp, peer, 0U);
        step(50000U);
        assert(bbp->sent.size() == 1U);
        bbp->getCallbacks()->onSubscribe(bbp, peer, 1U);
        step(); step(100U);
        assert(bbp->sent.size() == 2U);
        assert_page(bbp, 1U, 0xA0U);
        step(29999U);
        assert(bbp->sent.size() == 2U);
        step(1U);
        assert(bbp->sent.size() == 3U);
        assert_page(bbp, 2U, 0xA0U);
        puts("PASS: unsubscribe silences 30s A0; resubscribe sends a fresh header and resets period.");
    } else if (scenario == 11U) {
        /* [V1.4 BLE 30s 新增] BLE 傳送失敗沿用 100ms 限速，成功才重新計算 30s。 */
        test_notify_ok = false;
        const unsigned attempted_before = test_notify_attempts;
        step(30000U);
        assert(bbp->sent.size() == 1U);
        assert(test_notify_attempts == attempted_before + 1U);
        for (unsigned i = 0U; i < 10U; i++) { step(10U); }
        assert(test_notify_attempts == attempted_before + 2U);
        test_notify_ok = true;
        step(100U);
        assert(bbp->sent.size() == 2U);
        assert_page(bbp, 1U, 0xA0U);
        step(29999U);
        assert(bbp->sent.size() == 2U);
        step(1U);
        assert(bbp->sent.size() == 3U);
        assert_page(bbp, 2U, 0xA0U);
        assert(server->disconnect_requests.empty());
        puts("PASS: failed battery Notify retries are throttled and restart the period on success.");
    } else if (scenario == 12U) {
        /* [V1.4 BLE 30s 新增] uint32_t 時間計數回繞仍可定時發送。 */
        step(29999U);
        assert(bbp->sent.size() == 1U);
        step(1U);
        assert(bbp->sent.size() == 2U);
        assert_page(bbp, 1U, 0xA0U);
        puts("PASS: periodic A0 30s timer works across millis() rollover.");
    } else if (scenario == 13U) {
        /* [V1.4 LOAD Notify 新增] LOAD 改變不得插入曲線中，忙碌期間只回報最終去抖狀態。 */
        command(bbp, peer, 0x74U); step();
        assert_page(bbp, 1U, 0xB0U);
        background_ready = false;
        g_test_loaded = true; step();
        g_test_loaded = false; step();
        g_test_loaded = true; step();
        assert(bbp->sent.size() == 2U);
        background_ready = true;
        for (unsigned i = 0U; i < 11U; i++) { step(); }
        assert(bbp->sent.size() == 13U);
        for (size_t i = 1U; i < 13U; i++) {
            assert_page(bbp, i, i <= 8U ? static_cast<uint8_t>(0xAFU + i) :
                                      static_cast<uint8_t>(0x70U + i - 9U));
        }
        step();
        assert(bbp->sent.size() == 14U);
        assert_page(bbp, 13U, 0xA0U);
        assert((bbp->sent[13U][3U] & 0x04U) != 0U);
        puts("PASS: changing LOAD during 12-page dump produces only the latest post-dump A0.");
    } else if (scenario == 14U) {
        /* [V1.4 LOAD Notify 新增] 狀態封包失敗沿用原本 100ms 低功耗重試與不強制斷線。 */
        const unsigned before = test_notify_attempts;
        test_notify_ok = false;
        g_test_loaded = true;
        step(10U);
        assert(test_notify_attempts == before + 1U);
        step(99U);
        assert(test_notify_attempts == before + 1U);
        step(1U);
        assert(test_notify_attempts == before + 2U);
        test_notify_ok = true;
        step(100U);
        assert(bbp->sent.size() == 2U);
        assert_page(bbp, 1U, 0xA0U);
        assert((bbp->sent[1U][3U] & 0x04U) != 0U);
        step(100U);
        assert(bbp->sent.size() == 2U);
        assert(server->disconnect_requests.empty());
        puts("PASS: LOAD A0 failed Notify retries at 100ms with no disconnect or spam.");
    } else { assert(false); }
}
