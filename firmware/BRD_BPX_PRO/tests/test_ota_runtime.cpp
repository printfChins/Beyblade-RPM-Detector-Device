/* [V1.2 新增] 真實 OTA GATT/Flash adapter；替身只位於 MCU 硬體邊界。 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include "brd_ota.h"
#include "brd_config.h"
#include "brd_settings.h"
#include <esp_ota_ops.h>
#include <openssl/evp.h>
#include <assert.h>
#include <vector>
uint64_t test_clock_us = 10000U;
void (*test_interrupts[32])(void) = {};
bool test_notify_ok = true;
static bool safe = true, paused = false, charging = false, power = true;
static uint8_t soc = 80;
static unsigned resumes = 0;
static bool disconnect_in_verify = false;
bool brd_measurement_modifications_allowed() { return safe && !paused; }
bool brd_measurement_pause_for_settings() { if (!safe || paused) { return false; } paused = true; return true; }
void brd_measurement_resume_after_settings(bool) { paused = false; resumes++; }
bool brd_battery_measurement_allowed() { return power; }
uint8_t brd_battery_get_percent() { return soc; }
bool brd_io_is_charging() { return charging; }
bool brd_bbp_prepare_restart() { return true; }
bool brd_bbp_ota_ready() { return true; }
void brd_tft_update() {
    if (disconnect_in_verify) {
        disconnect_in_verify = false; brd_ota_disconnected(3); brd_ota_connected(3);
    }
}
const brd_settings::Manager &brd_settings_model() {
    static brd_settings::Manager model({0U, 0U, 30U, 2000U}, 30000U); return model;
}
static void put32(std::vector<uint8_t> &p, size_t off, uint32_t value) {
    for (unsigned i = 0; i < 4; i++) { p[off + i] = static_cast<uint8_t>(value >> (8U * i)); }
}
int main() {
    NimBLEServer server;
    assert(brd_ota_create(&server));
    auto *service = server.getService(OTA_SERVICE_UUID);
    auto *control = service->getCharacteristic(OTA_CONTROL_UUID);
    auto *input = service->getCharacteristic(OTA_DATA_UUID);
    auto *status = service->getCharacteristic(OTA_STATUS_UUID);
    NimBLEConnInfo conn(3);
    brd_ota_connected(3); status->getCallbacks()->onSubscribe(status, conn, 1);
    auto send = [&](NimBLECharacteristic *c, const std::vector<uint8_t> &p) {
        c->setValue(p.data(), p.size()); c->getCallbacks()->onWrite(c, conn); brd_ota_update();
        return status->getValue();
    };
    auto info = send(control, {1, 7});
    assert(info.size() == 20 && info[16] == 14 && info[19] == 1);
    std::vector<uint8_t> bytes(160, 0x55U);
    bytes[0] = 0xE9; bytes[12] = 5; bytes[13] = 0;
    put32(bytes, 32, 0xABCD5432U);
    memcpy(bytes.data() + 70, brd_ota::IMAGE_TAG, strlen(brd_ota::IMAGE_TAG));
    std::vector<uint8_t> hash(18); hash[0] = 2; hash[1] = 7;
    unsigned digest_len = 0;
    assert(EVP_Digest(bytes.data(), bytes.size(), hash.data() + 2, &digest_len, EVP_md5(), nullptr) == 1);
    auto start = std::vector<uint8_t>{3, 7, 0, 0, 0, 0}; put32(start, 2, bytes.size());
    assert(send(control, hash)[2] == brd_ota::OK);
    soc = 19; assert(send(control, start)[2] == brd_ota::POWER && test_flash_begins == 0);
    soc = 80; safe = false; assert(send(control, start)[2] == brd_ota::BUSY);
    safe = true;
    control->setValue(start.data(), start.size()); control->getCallbacks()->onWrite(control, conn);
    test_queue_receive_hook = []() { brd_ota_disconnected(3); brd_ota_connected(3); };
    brd_ota_update(); assert(test_flash_begins == 0 && !paused);
    status->getCallbacks()->onSubscribe(status, conn, 1);
    assert(send(control, hash)[2] == 0); assert(send(control, start)[2] == 0 && paused);
    assert(brd_ota_is_busy());
    brd_ota_disconnected(3); brd_ota_update(); assert(!paused && !brd_ota_is_busy());
    assert(test_flash_aborts == 1 && test_boot_switches == 0);
    brd_ota_connected(3); status->getCallbacks()->onSubscribe(status, conn, 1);
    auto transfer = [&](bool corrupt_hash) {
        auto h = hash; if (corrupt_hash) { h[2] ^= 1; }
        assert(send(control, h)[2] == 0); assert(send(control, start)[2] == 0);
        for (size_t off = 0; off < bytes.size(); off += 14) {
            size_t n = bytes.size() - off; if (n > 14) { n = 14; }
            std::vector<uint8_t> p(6 + n); p[0] = 16; p[1] = 7; put32(p, 2, off);
            memcpy(p.data() + 6, bytes.data() + off, n);
            assert(send(input, p)[2] == 0);
        }
        return send(control, {4, 7});
    };
    assert(transfer(true)[2] == brd_ota::VERIFY && test_boot_switches == 0 && !paused);
    test_flash_end_ok = false;
    assert(transfer(false)[2] == brd_ota::VERIFY && test_boot_switches == 0 && !paused);
    test_flash_end_ok = true;
    test_flash_corrupt_on_read = true;
    assert(transfer(false)[2] == brd_ota::VERIFY && test_boot_switches == 0 && !paused);
    test_flash_corrupt_on_read = false; test_flash_read_ok = false;
    assert(transfer(false)[2] == brd_ota::VERIFY && test_boot_switches == 0 && !paused);
    test_flash_read_ok = true;
    disconnect_in_verify = true;
    (void)transfer(false);
    assert(test_boot_switches == 0 && !paused && !brd_ota_is_busy());
    status->getCallbacks()->onSubscribe(status, conn, 1);
    test_ota_end_hook = []() { brd_ota_disconnected(3); brd_ota_connected(3); };
    (void)transfer(false);
    assert(test_boot_switches == 0 && !paused && !brd_ota_is_busy());
    status->getCallbacks()->onSubscribe(status, conn, 1);
    /* [V1.2 新增] 8 包 burst + 4 KiB Flash 緩衝與最後尾塊寫入驗證。 */
    test_negotiated_mtu = 247U; // [新增] 模擬 ATT MTU 247 -> DATA payload 238。
    std::vector<uint8_t> big(9001U, 0x55U);
    big[0] = 0xE9U; big[12] = 5U; big[13] = 0U;
    put32(big, 32, 0xABCD5432U);
    memcpy(big.data() + 70, brd_ota::IMAGE_TAG, strlen(brd_ota::IMAGE_TAG));
    std::vector<uint8_t> big_hash(18); big_hash[0] = 2U; big_hash[1] = 7U;
    assert(EVP_Digest(big.data(), big.size(), big_hash.data() + 2, &digest_len, EVP_md5(), nullptr) == 1);
    auto big_start = std::vector<uint8_t>{3, 7, 0, 0, 0, 0}; put32(big_start, 2, big.size());
    assert(send(control, big_hash)[2] == 0U);
    auto big_ready = send(control, big_start);
    assert(big_ready[2] == 0U && big_ready[16] == 238U && big_ready[17] == 0U);
    assert(send(control, {6, 7})[2] == 0U);
    assert(test_flash_writes > 0U); // earlier, 160-byte legacy tests did write a final block
    const unsigned before_writes = test_flash_writes;
    unsigned sync_count = 0U;
    for (size_t off = 0; off < big.size();) {
        size_t window_end = off;
        for (unsigned i = 0; i < 8U && window_end < big.size(); ++i) {
            size_t n = big.size() - window_end; if (n > 238U) { n = 238U; }
            std::vector<uint8_t> packet(6U + n);
            packet[0] = 16U; packet[1] = 7U; put32(packet, 2U, window_end);
            memcpy(packet.data() + 6U, big.data() + window_end, n);
            input->setValue(packet.data(), packet.size()); input->getCallbacks()->onWrite(input, conn);
            window_end += n;
        }
        const std::vector<uint8_t> sync_cmd = {6U, 7U};
        control->setValue(sync_cmd.data(), sync_cmd.size());
        control->getCallbacks()->onWrite(control, conn);
        /* 保證 Queue 中先入列的 8 筆 DATA 都處理完，SYNC 才回累計 offset。 */
        for (unsigned i = 0; i < 12U; ++i) { brd_ota_update(); }
        const auto response = status->getValue();
        assert(response[0] == 6U && response[2] == 0U);
        uint32_t reported = 0U;
        for (unsigned i = 0; i < 4U; i++) { reported |= uint32_t(response[4 + i]) << (8U * i); }
        assert(reported == window_end);
        sync_count++; off = window_end;
    }
    assert(sync_count >= 5U);
    assert(send(control, {4, 7})[3] == brd_ota::COMPLETE && test_boot_switches == 1 && paused);
    assert(test_flash_bytes == big);
    assert(test_flash_writes - before_writes == 3U); // 4096 + 4096 + 809

    assert(brd_ota_get_display().percent == 100 && brd_ota_is_busy());
    assert(send(control, {4, 7})[2] == 0 && test_boot_switches == 1);
    assert(send(control, {5, 7})[2] == brd_ota::BUSY);
    assert(resumes >= 3);
    puts("PASS: OTA GATT MTU, safe start, reconnect generation, MD5/image verification, app-only boot switch and measurement lease.");
}
