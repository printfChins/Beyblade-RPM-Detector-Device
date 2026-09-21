/* 檔案位置: BRD_BBP/tests/test_dual_edge.cpp
   [R3 修改] 雙邊沿更新 RPM，曲線以裝載後首個邊沿為固定參考，每圈一筆。
   預期值以手算週期給定，不使用待測函式建立預期結果。 */
#include <cstdint>
#include <cstdio>
#include "host_stubs/soc/gpio_struct.h"
#include "../brd_bbp.h"
uint32_t host_micros = 0, host_millis = 0;
void (*host_interrupts[32])() = {};
host_gpio_t GPIO{};
#include "../brd_bbp_protocol.cpp"
#include "../brd_bbp_session.cpp"
static brd_bbp::Session host_session;
bool brd_bbp_begin() { return true; }
void brd_bbp_stop() {}
bool brd_bbp_prepare_restart() { return true; }
void brd_bbp_update(bool) {}
bool brd_bbp_is_connected() { return false; }
brd_bbp_diagnostics_t brd_bbp_get_diagnostics() { return {}; }
void brd_bbp_capture_reset() { host_session.reset_capture(); }
void brd_bbp_capture_period(uint32_t p, uint32_t e, bool record) { host_session.period(p, e, record); }
void brd_bbp_capture_launch(uint32_t us) { host_session.launched(us); }
void brd_bbp_capture_finish() { host_session.finish(); }
void brd_bbp_capture_abort() { host_session.abort(); }
#include "../brd_measurement.cpp"
static int failures;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static uint32_t clock_base;
static void at(uint32_t us) {
    host_micros = us;
    host_millis = static_cast<uint32_t>(us - clock_base) / 1000U;
    brd_measurement_update();
}
static void set_pin(unsigned pin, int level) {
    const uint32_t mask = 1U << pin;
    GPIO.in.val = level ? GPIO.in.val | mask : GPIO.in.val & ~mask;
}
static void load(int level, uint32_t us) {
    set_pin(LOAD_IR_GPIO, level);
    host_micros = us;
    host_interrupts[LOAD_IR_GPIO]();
    at(us + 1000U);
}
static void edge(int level, uint32_t us, bool process = true) {
    int previous = (GPIO.in.val >> RPM_IR_GPIO) & 1U;
    set_pin(RPM_IR_GPIO, level);
    host_micros = us;
    // 依正式程式註冊的中斷模式送入硬體事件，FALLING 設定不會收到上升沿。
    if (previous != level && host_interrupts[RPM_IR_GPIO] != nullptr &&
        (host_interrupt_modes[RPM_IR_GPIO] == CHANGE ||
         (host_interrupt_modes[RPM_IR_GPIO] == FALLING && level == LOW))) {
        host_interrupts[RPM_IR_GPIO]();
    }
    if (process) {
        at(us);
    }
}
static void fresh(uint32_t base = 0, int initial_rpm_level = LOW) {
    brd_measurement_stop();
    brd_bbp::State empty;
    brd_bbp::clear(empty);
    host_session.restore(empty);
    clock_base = host_micros = base;
    host_millis = 0;
    GPIO.in.val = 0;
    set_pin(RPM_IR_GPIO, initial_rpm_level);
    brd_measurement_begin();
}
static void complete(uint32_t release_us, uint32_t last_edge_us, uint32_t publish_us) {
    load(LOW, release_us);
    at(last_edge_us + 300001U);
    host_session.update(publish_us, 600000U);
}
static void expect_curve(const uint16_t *values, unsigned size) {
    CHECK(host_session.state().total == 1);
    for (unsigned i = 0; i < 32; ++i) {
        CHECK(host_session.state().curve[i] == (i < size ? values[i] : 0U));
    }
}

static void test_same_polarity_periods_and_prelaunch_capture() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12250);
    CHECK(brd_measurement_get_display().value == 0); // 只有半圈，不能算 RPM。
    edge(HIGH, 18000);
    CHECK(brd_measurement_get_display().value == 10000); // 上升到上升 6000 us。
    edge(LOW, 20250);
    CHECK(brd_measurement_get_display().value == 7500); // 下降到下降 8000 us。
    edge(HIGH, 24000);
    CHECK(brd_measurement_get_display().value == 10000);
    edge(LOW, 28250);
    CHECK(brd_measurement_get_display().value == 7500);
    complete(29000, 28250, 618000);
    const uint16_t expected[] = {750, 750};
    expect_curve(expected, 2); // 正緣先到，只保存正緣整圈；發射前資料保留。
    CHECK(host_session.state().records[0] == 10000);
}

static void test_narrow_pulse_not_half_period_rpm() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12050); // 50 us 脈寬不能套用整圈的最小週期過濾。
    edge(HIGH, 16000);
    edge(LOW, 16050);
    CHECK(brd_measurement_get_display().value == 15000);
    complete(17000, 16050, 616000);
    const uint16_t expected[] = {500};
    expect_curve(expected, 1);
}

static void test_unloaded_edges_do_not_seed_next_shot() {
    fresh();
    edge(HIGH, 1000);
    edge(LOW, 1100);
    edge(HIGH, 7000);
    edge(LOW, 7100);
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12100);
    CHECK(brd_measurement_get_display().value == 0);
    edge(HIGH, 18000);
    edge(LOW, 18100);
    complete(19000, 18100, 618000);
    const uint16_t expected[] = {750};
    expect_curve(expected, 1);
}

static void test_zero_timeout_resets_both_period_bases() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12100);
    edge(HIGH, 18000);
    edge(LOW, 18100);
    at(318101);
    CHECK(brd_measurement_get_display().value == 0);
    edge(HIGH, 400000);
    edge(LOW, 400100);
    CHECK(brd_measurement_get_display().value == 0);
    edge(HIGH, 406000);
    edge(LOW, 406100);
    CHECK(brd_measurement_get_display().value == 10000);
    complete(407000, 406100, 800000);
    const uint16_t expected[] = {750, 750};
    expect_curve(expected, 2);
}

static void test_first_32_revolutions_are_frozen() {
    fresh();
    load(HIGH, 10000);
    uint32_t time_us = 12000;
    for (unsigned i = 0; i < 34; ++i) {
        if (i != 0) {
            time_us += i <= 16 ? 6000U : (i <= 32 ? 8000U : 12000U);
        }
        edge(HIGH, time_us);
        edge(LOW, time_us + 200U);
    }
    edge(HIGH, 251000); // 第 34 圈變成 3000 us，更新 MAX 但不能重寫前 32 圈。
    edge(LOW, 251200);
    complete(252000, 251200, 700000);
    CHECK(host_session.state().total == 1 && host_session.state().records[0] == 20000);
    for (unsigned i = 0; i < 32; ++i) {
        // 前 16 圈 6000 us，後 16 圈 8000 us；重複收雙邊沿會漏掉後半段。
        CHECK(host_session.state().curve[i] == (i < 16 ? 750 : 1000));
    }
}

static void test_event_polarity_survives_queued_batch() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000, false);
    edge(LOW, 12100, false);
    edge(HIGH, 18000, false);
    edge(LOW, 18100, false);
    at(19000); // GPIO 已是 LOW；必須使用 ISR 保存的每筆電位。
    complete(20000, 18100, 618000);
    const uint16_t expected[] = {750};
    expect_curve(expected, 1);
}

static void test_dual_edge_periods_across_micros_wrap() {
    fresh(0xffff0000U);
    load(HIGH, 0xffff1000U);
    edge(HIGH, 0xfffff000U);
    edge(LOW, 0xfffff080U);
    edge(HIGH, 0x00000770U); // 6000 us，上升沿跨回繞。
    edge(LOW, 0x000007f0U); // 6000 us，下降沿跨回繞。
    complete(0x00001000U, 0x000007f0U, 0x000b0000U);
    const uint16_t expected[] = {750};
    expect_curve(expected, 1);
}

static void test_short_same_polarity_glitches_do_not_move_period_base() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12050);
    edge(HIGH, 16000);
    edge(LOW, 16050);
    edge(HIGH, 16070); // 與上次有效上升沿相距 70 us，應忽略且不移動基準。
    edge(LOW, 16090);  // 與上次有效下降沿相距 40 us。
    edge(HIGH, 20000);
    edge(LOW, 20050);
    complete(21000, 20050, 616000);
    const uint16_t expected[] = {500, 500};
    expect_curve(expected, 2);
}

static void test_reload_clears_both_polarity_bases() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12100);
    edge(HIGH, 18000);
    edge(LOW, 18100);
    complete(19000, 18100, 618000);
    at(3200000); // 解除前一發 HOLD，重新同步 LOAD。
    load(HIGH, 3210000);
    edge(HIGH, 3212000);
    edge(LOW, 3212100);
    CHECK(brd_measurement_get_display().value == 0);
    edge(HIGH, 3216000);
    edge(LOW, 3216100);
    CHECK(brd_measurement_get_display().value == 15000);
    complete(3217000, 3216100, 3816000);
    CHECK(host_session.state().total == 2);
    CHECK(host_session.state().records[1] == 15000);
    CHECK(host_session.state().curve[0] == 500);
    CHECK(host_session.state().curve[1] == 0);
}

static void test_dual_edge_overflow_reset_and_reload() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12100);
    edge(HIGH, 18000);
    edge(LOW, 18100);
    // 交替邊沿不交給主迴圈處理，確實超出 255 個事件的有效容量。
    for (unsigned i = 0; i < 258; ++i) {
        edge((i % 2U) ? LOW : HIGH, 20000U + i * 500U, false);
    }
    at(149000);
    CHECK(brd_measurement_get_diagnostics().rpm_queue_overflows == 1);
    CHECK(brd_measurement_get_display().value == 0);
    edge(HIGH, 150000);
    edge(LOW, 150100);
    CHECK(brd_measurement_get_display().value == 0); // 兩類邊沿重新建立基準。
    edge(HIGH, 156000);
    edge(LOW, 156100);
    CHECK(brd_measurement_get_display().value == 10000);
    complete(157000, 156100, 800000);
    CHECK(host_session.state().total == 0); // 溢位的這一發不能發布。
    at(3200000);
    load(HIGH, 3210000);
    edge(HIGH, 3212000);
    edge(LOW, 3212100);
    CHECK(brd_measurement_get_display().value == 0);
    edge(HIGH, 3218000);
    edge(LOW, 3218100);
    complete(3219000, 3218100, 3818000);
    const uint16_t expected[] = {750};
    expect_curve(expected, 1);
}

static void test_falling_first_selects_falling_revolutions() {
    fresh(0, HIGH); // 初始電位不等於參考邊沿；首個實際觸發是下降沿。
    load(HIGH, 10000);
    edge(LOW, 12000);
    edge(HIGH, 12250);
    edge(LOW, 18000);
    CHECK(brd_measurement_get_display().value == 10000);
    edge(HIGH, 20250);
    CHECK(brd_measurement_get_display().value == 7500);
    edge(LOW, 24000);
    edge(HIGH, 28250);
    complete(29000, 28250, 618000);
    const uint16_t expected[] = {750, 750};
    expect_curve(expected, 2);
}

static void test_nonreference_edge_still_updates_representative_max() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 14250);
    edge(HIGH, 20000); // 參考正緣 8000 us，7500 RPM。
    edge(LOW, 20250);  // 非參考負緣 6000 us，10000 RPM。
    CHECK(brd_measurement_get_display().value == 10000);
    complete(21000, 20250, 620000);
    const uint16_t expected[] = {1000};
    expect_curve(expected, 1);
    CHECK(host_session.state().records[0] == 10000);
}

static void test_first_trigger_is_reference_not_first_valid_rpm() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000); // 首個觸發選正緣。
    edge(LOW, 12050);
    edge(HIGH, 12500); // 過快，不能形成有效正緣轉速。
    edge(LOW, 13550);  // 第一筆有效 RPM 來自負緣，也不能改參考。
    edge(HIGH, 16000);
    edge(LOW, 16050);
    complete(17000, 16050, 613550);
    const uint16_t expected[] = {500};
    expect_curve(expected, 1);
    CHECK(host_session.state().records[0] == 40000);
}

static void test_reload_can_select_opposite_reference() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12250);
    edge(HIGH, 18000);
    edge(LOW, 20250);
    complete(21000, 20250, 618000);
    at(3200000);
    edge(HIGH, 3202000); // 未裝載事件不得選擇下一發參考。
    load(HIGH, 3210000);
    edge(LOW, 3212000);
    edge(HIGH, 3212250);
    edge(LOW, 3218000);
    edge(HIGH, 3220250);
    complete(3221000, 3220250, 3818000);
    CHECK(host_session.state().total == 2);
    CHECK(host_session.state().curve[0] == 750);
    CHECK(host_session.state().curve[1] == 0);
}

static void test_timeout_keeps_reference_until_capture_reset() {
    fresh();
    load(HIGH, 10000);
    edge(HIGH, 12000);
    edge(LOW, 12100);
    edge(HIGH, 18000);
    edge(LOW, 18100);
    edge(HIGH, 24000); // 最後狀態為 HIGH，暫停後先來 LOW。
    at(324001);
    edge(LOW, 400000);
    edge(HIGH, 400250);
    edge(LOW, 406000);
    edge(HIGH, 408250); // 同一發仍是正緣參考，8000 us。
    complete(409000, 408250, 800000);
    const uint16_t expected[] = {750, 750, 1000};
    expect_curve(expected, 3);
}

int main() {
    test_same_polarity_periods_and_prelaunch_capture();
    test_narrow_pulse_not_half_period_rpm();
    test_unloaded_edges_do_not_seed_next_shot();
    test_zero_timeout_resets_both_period_bases();
    test_first_32_revolutions_are_frozen();
    test_event_polarity_survives_queued_batch();
    test_dual_edge_periods_across_micros_wrap();
    test_short_same_polarity_glitches_do_not_move_period_base();
    test_reload_clears_both_polarity_bases();
    test_dual_edge_overflow_reset_and_reload();
    test_falling_first_selects_falling_revolutions();
    test_nonreference_edge_still_updates_representative_max();
    test_first_trigger_is_reference_not_first_valid_rpm();
    test_reload_can_select_opposite_reference();
    test_timeout_keeps_reference_until_capture_reset();
    return failures ? 1 : 0;
}
