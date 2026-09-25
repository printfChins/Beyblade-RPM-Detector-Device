/*
    檔案位置: BRD_BBP/tests/test_subscriber.cpp
    [BRD_BBP 修正測試 新增] 執行真正的 BLE 排程與協定核心。
    重現可連線但沒有資料: 手機只訂閱通知，全程不寫入指令。
*/
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "host_stubs/Arduino.h"
#include "host_stubs/Preferences.h"
#include "host_stubs/NimBLEDevice.h"
#include "../brd_measurement.h"

uint32_t host_micros = 0, host_millis = 0;
void (*host_interrupts[32])() = {};
static bool loaded = false;

bool brd_battery_measurement_allowed() { return true; }
uint8_t brd_battery_get_percent() { return 50; }
brd_display_t brd_measurement_get_display() {
    return {loaded, false, 0, 0, false};
}

#include "../brd_bbp_protocol.cpp"
#include "../brd_bbp_session.cpp"
#include "../brd_bbp.cpp"

static int failures = 0;
#define CHECK(x) do { if (!(x)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    ++failures; \
} } while (0)

static void tick(uint32_t ms) {
    host_millis = ms;
    host_micros = ms * 1000;
    brd_bbp_update(true);
}

static void complete_shot(uint32_t first_us) {
    brd_bbp_capture_reset();
    brd_bbp_capture_period(6000, first_us);
    brd_bbp_capture_launch(first_us);
    // [R3 新增] 經正式 BLE wrapper 的非參考邊沿只更新 MAX，不佔曲線格。
    brd_bbp_capture_period(4000, first_us + 500, false);
    brd_bbp_capture_period(12000, first_us + 12000);
    brd_bbp_capture_finish();
}

static void check_dump(size_t first, uint16_t total) {
    CHECK(host_ble::notifications.size() >= first + 12);
    if (host_ble::notifications.size() < first + 12) {
        return;
    }
    uint8_t checksum = 0;
    for (size_t i = 0; i < 12; ++i) {
        const auto &page = host_ble::notifications[first + i];
        CHECK(page.size() == 17);
        CHECK(page[0] == (i < 8 ? 0xb0 + i : 0x70 + i - 8));
        if (i < 7) {
            for (size_t j = 1; j < page.size(); ++j) {
                checksum = static_cast<uint8_t>(checksum + page[j]);
            }
        }
    }
    CHECK(host_ble::notifications[first + 7][16] == checksum);
    const auto &metadata = host_ble::notifications[first + 6];
    CHECK((metadata[9] | (metadata[10] << 8)) == total);
    CHECK((metadata[7] | (metadata[8] << 8)) == 15000);
    /* [新增] 裝載後 6000 us / 8 = 750；主動通知仍使用 raw period 格式。 */
    const auto &profile = host_ble::notifications[first + 8];
    CHECK((profile[1] | (profile[2] << 8)) == 750);
    CHECK((profile[3] | (profile[4] << 8)) == 1500);
    CHECK(profile[5] == 0 && profile[6] == 0);
}

int main() {
    host_nvs::blob.clear();
    CHECK(brd_bbp_begin());
    NimBLEDevice::server.hostConnect(7);
    tick(0);
    CHECK(host_ble::notifications.empty());
    NimBLEDevice::server.s.c.hostSubscribe(7, 1);
    tick(0);
    tick(99);
    CHECK(host_ble::notifications.empty());
    tick(100);
    CHECK(host_ble::notifications.size() == 1);
    if (!host_ble::notifications.empty()) {
        CHECK(host_ble::notifications.back()[0] == 0xa0);
        CHECK(host_ble::notifications.back()[3] == 0);
    }

    /* [新增] 尚未轉動也必須回報裝載；卸載後回報 0x00。 */
    loaded = true;
    tick(110);
    CHECK(host_ble::notifications.size() == 2);
    if (!host_ble::notifications.empty()) {
        CHECK(host_ble::notifications.back()[3] == 4);
    }
    loaded = false;
    tick(120);
    CHECK(host_ble::notifications.size() == 3);
    if (!host_ble::notifications.empty()) {
        CHECK(host_ble::notifications.back()[3] == 0);
    }

    complete_shot(200000);
    tick(799);
    CHECK(host_ble::notifications.size() == 3);
    for (uint32_t ms = 800; ms <= 910; ms += 10) {
        tick(ms);
    }
    CHECK(host_ble::notifications.size() == 15);
    check_dump(3, 1);
    tick(950);
    CHECK(host_ble::notifications.size() == 15);

    /* [新增] 資料頁傳輸期間的 LOAD 變化須等整批結束，不插入 A0。 */
    complete_shot(1000000);
    tick(1600);
    loaded = true;
    for (uint32_t ms = 1610; ms <= 1710; ms += 10) {
        tick(ms);
    }
    check_dump(15, 2);
    tick(1720);
    CHECK(host_ble::notifications.size() == 28);
    if (!host_ble::notifications.empty()) {
        CHECK(host_ble::notifications.back()[0] == 0xa0);
        CHECK(host_ble::notifications.back()[3] == 4);
    }

    /* [新增] notify 失敗須保留同一張主動通知，成功後不再重送。 */
    host_ble::notify_results = {false, true};
    host_ble::notify_index = 0;
    loaded = false;
    tick(1730);
    tick(1740);
    CHECK(host_ble::notifications.size() == 30);
    if (host_ble::notifications.size() >= 30) {
        CHECK(host_ble::notifications[28] == host_ble::notifications[29]);
    }

    /* [新增] 中途取消訂閱後重新訂閱，只送新 A0，不續送舊批次尾頁。 */
    complete_shot(1800000);
    tick(2400);
    const size_t before_unsubscribe = host_ble::notifications.size();
    NimBLEDevice::server.s.c.hostSubscribe(7, 0);
    tick(2410);
    CHECK(host_ble::notifications.size() == before_unsubscribe);
    NimBLEDevice::server.s.c.hostSubscribe(7, 1);
    tick(2420);
    tick(2519);
    CHECK(host_ble::notifications.size() == before_unsubscribe);
    tick(2520);
    tick(2530);
    CHECK(host_ble::notifications.size() == before_unsubscribe + 1);
    if (!host_ble::notifications.empty()) {
        CHECK(host_ble::notifications.back()[0] == 0xa0);
    }

    /* [新增] 相容模式仍支援主機主動要求 A0 與完整 12 頁。 */
    const size_t before_commands = host_ble::notifications.size();
    NimBLEDevice::server.s.c.hostWrite(std::string(1, '\x51'), 7);
    tick(2540);
    CHECK(host_ble::notifications.size() == before_commands + 1);
    NimBLEDevice::server.s.c.hostWrite(std::string(1, '\x74'), 7);
    for (uint32_t ms = 2550; ms <= 2660; ms += 10) {
        tick(ms);
    }
    check_dump(before_commands + 1, 3);
    CHECK(host_ble::notifications.size() == before_commands + 13);

    /* [新增] 新結果發布同輪收到清除，不得再主動送出已被清除的結果。 */
    complete_shot(2800000);
    const size_t before_clear = host_ble::notifications.size();
    NimBLEDevice::server.s.c.hostWrite(std::string(1, '\x75'), 7);
    tick(3400);
    tick(3410);
    CHECK(host_ble::notifications.size() == before_clear);
    CHECK(g_session.state().total == 0);

    /* [新增] 初始 A0 與 0x51 可共用一次回覆，等待期間不額外送一份 A0。 */
    NimBLEDevice::server.s.c.hostSubscribe(7, 0);
    tick(4000);
    NimBLEDevice::server.s.c.hostSubscribe(7, 1);
    NimBLEDevice::server.s.c.hostWrite(std::string(1, '\x51'), 7);
    tick(4010);
    CHECK(host_ble::notifications.size() == before_clear);
    tick(4110);
    tick(4120);
    CHECK(host_ble::notifications.size() == before_clear + 1);
    return failures ? 1 : 0;
}
