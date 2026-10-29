/* 檔案: BRD_BPX_PRO/tests/test_dual_edge_ble_selection.cpp
 * [新增] 兩組同極性完整單圈曲線，比較整發 MAX 選擇對應 BLE 32 點。
 * [驗證] 沒有改變 BBP 17-byte / 12-page 格式或 181-byte NVS 編碼。
 */
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_bbp_session.h"

static uint16_t latest(const brd_bbp::Session &session) {
    const auto &s = session.state();
    return s.records[(s.next + brd_bbp::HISTORY_SIZE - 1U) % brd_bbp::HISTORY_SIZE];
}
static uint16_t get16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
int main() {
    brd_bbp::Session session;
    // 負緣 ~15000，正緣 ~20000：正緣必須被選中。
    for (unsigned i = 0; i < 40; ++i) {
        session.period(4000U, 1000000UL + i * 6000U, 0U);
        session.period(3000U, 1002500UL + i * 6000U, 1U);
    }
    session.period(2000U, 1400000UL, 1U); // 正緣最高 30000，位於曲線 32 點之外
    session.launched(1410000UL);
    session.finish();
    assert(session.pending());
    session.update(1800000UL, 600000UL);
    assert(session.state().count == 1U && latest(session) == 30000U);
    assert(session.state().curve[0] == 375U); // 正緣 3000us / 8，非負緣 500
    assert(session.state().curve[31] == 375U);
    uint8_t pages[brd_bbp::PAGE_COUNT][brd_bbp::PACKET_SIZE] = {};
    brd_bbp::build_dump(session.state(), pages);
    assert(pages[8][0] == 0x70 && get16(pages[8] + 1) == 375U);
    uint8_t storage[brd_bbp::STORAGE_SIZE] = {};
    brd_bbp::encode_storage(session.state(), storage);
    brd_bbp::State restored;
    assert(brd_bbp::decode_storage(storage, sizeof(storage), restored));
    assert(restored.curve[0] == 375U && restored.lifetime_max == 30000U);

    // 第二發改由負緣勝出，證明兩次選擇不會混用上一發資料。
    session.reset_capture();
    for (unsigned i = 0U; i < 4U; ++i) {
        session.period(2500U, 2000000UL + 5000U*i, 0U); // 24000
        session.period(5000U, 2002500UL + 5000U*i, 1U); // 12000
    }
    session.launched(2024000UL);
    session.finish();
    session.update(2900000UL, 600000UL);
    assert(session.state().count == 2U && latest(session) == 24000U);
    assert(session.state().curve[0] == 313U); // 2500 / 8 四捨五入 = 313
    assert(session.state().curve[4] == 0U);
    puts("PASS: dual-polarity complete-revolution profiles, MAX winning selection, BBP/NVS compatibility.");
}
