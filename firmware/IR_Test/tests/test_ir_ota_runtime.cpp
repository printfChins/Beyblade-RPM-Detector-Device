/*
 * [新增] IR_Test OTA 完整 GATT -> Queue -> Flash 邊界主機測試。
 * 用測試替身取代 ESP32/NimBLE 硬體，並非實際 ESP32 編譯或 OTA 實機測試。
 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_ota_ops.h>
#include <openssl/evp.h>
#include <assert.h>
#include <cstring>
#include <cstdio>
#include <vector>

#include "ir_test_ota.h"
#include "ir_test_ota_config.h"
#include "ir_test_ota_guard.h"

uint64_t test_clock_us = 10000000ULL;
void (*test_interrupts[32])() = {};
bool test_notify_ok = true;
static unsigned test_ui_verify_count = 0U;

void irTestOtaRenderNow()
{
    assert(irTestOtaGetDisplay().verifying);
    test_ui_verify_count++;
}

static void put32(std::vector<uint8_t> &p, size_t offset, uint32_t value)
{
    for (unsigned i = 0; i < 4U; i++) {
        p[offset + i] = static_cast<uint8_t>(value >> (8U * i));
    }
}

static std::vector<uint8_t> firmware(bool irTag, size_t size)
{
    std::vector<uint8_t> image(size, 0x55U);
    image[0] = 0xE9U;
    image[12] = 5U;
    image[13] = 0U;
    put32(image, 32U, 0xABCD5432U);
    memcpy(image.data() + 60U, brd_ota::IMAGE_TAG, strlen(brd_ota::IMAGE_TAG));
    if (irTag) {
        memcpy(image.data() + 110U, IR_TEST_OTA_IMAGE_TAG, strlen(IR_TEST_OTA_IMAGE_TAG));
    }
    return image;
}

int main()
{
    /* [新增] 沿用原始 IR_Test 分區容量，未進行任何擴容。 */
    assert(test_next_partition.size == 0x140000U);
    irTestOtaSetBatteryMv(3800U);
    assert(irTestOtaBegin());
    NimBLEServer *server = NimBLEDevice::getServer();
    auto *service = server->getService(IR_OTA_SERVICE_UUID);
    auto *control = service->getCharacteristic(IR_OTA_CONTROL_UUID);
    auto *input = service->getCharacteristic(IR_OTA_DATA_UUID);
    auto *status = service->getCharacteristic(IR_OTA_STATUS_UUID);
    NimBLEConnInfo conn(3U);
    server->callbacks->onConnect(server, conn);
    status->getCallbacks()->onSubscribe(status, conn, 1U);

    auto send = [&](NimBLECharacteristic *c, const std::vector<uint8_t> &p) {
        c->setValue(p.data(), p.size());
        c->getCallbacks()->onWrite(c, conn);
        irTestOtaUpdate();
        return status->getValue();
    };

    /* [V1.2.1 新增] 空封包不得排入 OTA 佇列，也不得改變回覆狀態。 */
    const auto before_empty_status = status->getValue();
    control->setValue(nullptr, 0U);
    control->getCallbacks()->onWrite(control, conn);
    irTestOtaUpdate();
    const auto after_empty_status = status->getValue();
    assert(after_empty_status.size() == before_empty_status.size());
    for (size_t i = 0U; i < before_empty_status.size(); i++) {
        assert(after_empty_status[i] == before_empty_status[i]);
    }

    assert(send(control, {brd_ota::INFO, 7U})[2] == brd_ota::OK);
    assert(status->getValue()[19] == 1U);
    assert(status->getValue()[18] & brd_ota::FEATURE_BATCH_ACK);

    auto transfer = [&](const std::vector<uint8_t> &image, bool corrupt = false) {
        std::vector<uint8_t> hash(18U);
        hash[0] = brd_ota::HASH;
        hash[1] = 7U;
        unsigned output_length = 0U;
        assert(EVP_Digest(image.data(), image.size(), hash.data() + 2U, &output_length,
                          EVP_md5(), nullptr) == 1);
        assert(output_length == 16U);
        if (corrupt) {
            hash[2] ^= 0x41U;
        }
        std::vector<uint8_t> begin = {brd_ota::BEGIN, 7U, 0U, 0U, 0U, 0U};
        put32(begin, 2U, image.size());
        assert(send(control, hash)[2] == brd_ota::OK);
        assert(send(control, begin)[2] == brd_ota::OK);
        assert(irTestOtaBusy());
        for (size_t offset = 0U; offset < image.size();) {
            const size_t n = (image.size() - offset > 14U) ? 14U : image.size() - offset;
            std::vector<uint8_t> frame(6U + n);
            frame[0] = brd_ota::DATA;
            frame[1] = 7U;
            put32(frame, 2U, offset);
            memcpy(frame.data() + 6U, image.data() + offset, n);
            assert(send(input, frame)[2] == brd_ota::OK);
            offset += n;
        }
        return send(control, {brd_ota::END, 7U});
    };

    /* [新增] 低電拒絕 BEGIN，不能擦除 Flash。 */
    const auto small = firmware(true, 170U);
    std::vector<uint8_t> digest(18U);
    digest[0] = brd_ota::HASH;
    digest[1] = 7U;
    unsigned digest_len = 0;
    assert(EVP_Digest(small.data(), small.size(), digest.data() + 2,
                      &digest_len, EVP_md5(), nullptr) == 1);
    std::vector<uint8_t> begin = {brd_ota::BEGIN, 7U, 0U, 0U, 0U, 0U};
    put32(begin, 2U, small.size());
    irTestOtaSetBatteryMv(3500U);
    assert(send(control, digest)[2] == brd_ota::OK);
    assert(send(control, begin)[2] == brd_ota::POWER);
    assert(test_flash_begins == 0U);
    irTestOtaSetBatteryMv(3800U);

    /* [新增] 沒有專屬 IR_TEST 標記的 BPX 映像：Flash 驗證拒絕，不能切換 APP。 */
    assert(transfer(firmware(false, 170U))[2] == brd_ota::VERIFY);
    assert(test_boot_switches == 0U);
    assert(test_flash_aborts >= 1U);

    /* [新增] 錯誤 MD5：不得提交開機分區。 */
    assert(transfer(small, true)[2] == brd_ota::VERIFY);
    assert(test_boot_switches == 0U);

    /* [新增] 傳輸中斷線：未完成 APP 不生效、下次從頭傳。 */
    assert(send(control, digest)[2] == brd_ota::OK);
    assert(send(control, begin)[2] == brd_ota::OK);
    server->callbacks->onDisconnect(server, conn, 19);
    irTestOtaUpdate();
    assert(!irTestOtaBusy());
    assert(test_boot_switches == 0U);
    server->callbacks->onConnect(server, conn);
    status->getCallbacks()->onSubscribe(status, conn, 1U);

    /* [新增] 故障的 Flash 讀回必須阻止分區切換。 */
    test_flash_corrupt_on_read = true;
    assert(transfer(small)[2] == brd_ota::VERIFY);
    assert(test_boot_switches == 0U);
    test_flash_corrupt_on_read = false;

    /* [新增] 更新中掉電：寫入回應失敗，未完成映像不得切換開機分區。 */
    assert(send(control, digest)[2] == brd_ota::OK);
    put32(begin, 2U, small.size());
    assert(send(control, begin)[2] == brd_ota::OK);
    irTestOtaSetBatteryMv(3500U);
    std::vector<uint8_t> first(6U + 14U);
    first[0] = brd_ota::DATA;
    first[1] = 7U;
    put32(first, 2U, 0U);
    memcpy(first.data() + 6U, small.data(), 14U);
    assert(send(input, first)[2] == brd_ota::WRITE);
    assert(test_boot_switches == 0U);
    irTestOtaSetBatteryMv(3800U);

    /* [新增] 4 KiB 緩衝 + 尾塊寫入；SYNC 批次 ACK，成功後更新 APP。 */
    test_negotiated_mtu = 247U;
    const auto large = firmware(true, 9001U);
    std::vector<uint8_t> hash(18U);
    hash[0] = brd_ota::HASH;
    hash[1] = 7U;
    assert(EVP_Digest(large.data(), large.size(), hash.data() + 2,
                      &digest_len, EVP_md5(), nullptr) == 1);
    put32(begin, 2U, large.size());
    assert(send(control, hash)[2] == brd_ota::OK);
    assert(send(control, begin)[2] == brd_ota::OK);
    assert(send(control, {brd_ota::SYNC, 7U})[2] == brd_ota::OK);
    const unsigned before_writes = test_flash_writes;
    for (size_t offset = 0U; offset < large.size();) {
        size_t next = offset;
        for (unsigned i = 0U; i < 8U && next < large.size(); i++) {
            size_t n = large.size() - next;
            if (n > 238U) {
                n = 238U;
            }
            std::vector<uint8_t> p(6U + n);
            p[0] = brd_ota::DATA;
            p[1] = 7U;
            put32(p, 2U, next);
            memcpy(p.data() + 6U, large.data() + next, n);
            input->setValue(p.data(), p.size());
            input->getCallbacks()->onWrite(input, conn);
            next += n;
        }
        const std::vector<uint8_t> sync = {brd_ota::SYNC, 7U};
        control->setValue(sync.data(), sync.size());
        control->getCallbacks()->onWrite(control, conn);
        for (unsigned i = 0U; i < 11U; i++) {
            irTestOtaUpdate();
        }
        auto report = status->getValue();
        uint32_t reported = 0U;
        for (unsigned i = 0U; i < 4U; i++) {
            reported |= static_cast<uint32_t>(report[4U + i]) << (8U * i);
        }
        assert(report[0] == brd_ota::SYNC && report[2] == brd_ota::OK && reported == next);
        offset = next;
    }
    auto finish = send(control, {brd_ota::END, 7U});
    assert(finish[2] == brd_ota::OK && finish[3] == brd_ota::COMPLETE);
    assert(test_boot_switches == 1U);
    assert(test_ui_verify_count > 0U);
    assert(test_flash_bytes == large);
    assert(test_flash_writes - before_writes == 3U);
    assert(irTestOtaGetDisplay().percent == 100U);
    assert(irTestOtaBusy());
    assert(send(control, {brd_ota::ABORT, 7U})[2] == brd_ota::BUSY);
    puts("PASS: BLE OTA link, battery gate, image guard, MD5/Flash readback, disconnect, 4KiB buffering, SYNC and boot switch");
}
