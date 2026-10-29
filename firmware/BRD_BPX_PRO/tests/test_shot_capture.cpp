/*
 * 檔案位置：BRD_BPX_PRO/tests/test_shot_capture.cpp
 * [新增] 高速完整逐圈紀錄、先測速後畫圖、全程 64/32 點、MAX、雙緣與溢位。
 */
#include <cassert>
#include <cstdio>
#include "brd_config.h"
#include "brd_shot_capture.h"
#include "brd_ui_curve.h"
#include "brd_bbp_session.h"
#include "brd_bbp.h"

static brd_bbp::Session session;
void brd_bbp_capture_period(uint32_t us, uint32_t time_us, uint8_t edge) {
    session.period(us, time_us, edge);
}
void brd_bbp_capture_set_max(uint16_t rpm) { session.set_representative_max(rpm); }
void brd_bbp_capture_finish() { session.finish(); }

static void test_high_speed() {
    session.reset_capture();
    brd_ui::curve_begin();
    brd_shot::reset();
    const uint32_t start = 1000000U;
    session.launched(start + 10000U);
    for (uint16_t i = 0U; i < 3000U; ++i) {
        /* 兩組共 3000 點，原 V1.3 的 50ms sampling 在 3 秒只能記 60 點。 */
        uint8_t edge = static_cast<uint8_t>(i & 1U);
        uint32_t period = edge == 1U ? 2000U : 2500U;
        uint16_t rpm = edge == 1U ? 30000U : 24000U;
        if (i == 2801U) { period = 1600U; rpm = 37500U; }
        brd_shot::record(period, start + static_cast<uint32_t>(i) * 1000U, edge, rpm);
    }
    assert(brd_shot::diagnostics().recorded == 3000U);
    assert(!brd_shot::diagnostics().truncated);
    assert(brd_ui::curve_snapshot().count == 0U);
    assert(session.state().count == 0U);
    brd_shot::finish(37500U);
    assert(brd_shot::diagnostics().pending);
    assert(brd_ui::curve_snapshot().count == 0U);
    assert(brd_shot::publish_finished());
    assert(!brd_shot::publish_finished());
    const auto &curve = brd_ui::curve_snapshot();
    assert(curve.frozen && curve.count == 64U && curve.peak == 37500U);
    bool peak_seen = false;
    for (uint8_t i = 0U; i < curve.count; ++i) {
        if (curve.rpm[i] == 37500U) { peak_seen = true; }
    }
    assert(peak_seen);
    /* 選正緣完整一圈，32 點內必須包含後段 MAX。 */
    assert(session.pending());
    session.update(start + 4000000U, 600000U);
    assert(session.state().lifetime_max == 37500U);
    assert(session.state().count == 1U);
    peak_seen = false;
    for (uint8_t i = 0U; i < brd_bbp::CURVE_SIZE; ++i) {
        if (session.state().curve[i] == 200U) { peak_seen = true; }
        assert(session.state().curve[i] != 313U); /* 負緣 period 2500 不允許選中 */
    }
    assert(peak_seen);
}
static void test_capacity() {
    session.reset_capture();
    brd_shot::reset();
    brd_ui::curve_capture_reset();
    for (uint32_t i = 0U; i < SHOT_RAW_SAMPLE_CAPACITY + 10U; ++i) {
        brd_shot::record(2000U, 5000000UL + 1000UL*i, 1U, 30000U);
    }
    assert(brd_shot::diagnostics().recorded == SHOT_RAW_SAMPLE_CAPACITY);
    assert(brd_shot::diagnostics().dropped == 10U);
    assert(brd_shot::diagnostics().truncated);
    brd_shot::finish(30000U);
    session.launched(5000000UL);
    assert(brd_shot::publish_finished());
    assert(brd_ui::curve_snapshot().count == 64U);
    assert(brd_ui::curve_snapshot().peak == 30000U);
    brd_shot::reset();
    assert(!brd_shot::diagnostics().truncated);
}
int main() {
    test_high_speed();
    test_capacity();
    puts("PASS: RPM-first raw capture (3000+8192 edges), deferred 64/32 points, MAX preservation and overflow diagnostic.");
}
