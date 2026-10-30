/*
    [V1.2.1 新增] 檔案：BRD_BPX/tests/test_battery_curve.cpp
    使用原 brd_battery.cpp + ADC stub，驗證只移植放電曲線與充電補償。
    不測、不依賴 FULL；低電鎖定仍是原 V1.2 百分比模式。
*/
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include "brd_battery.h"
#include "brd_config.h"

uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
int test_battery_mv = 4000;
bool test_adc_fail = false;
bool test_charging = false;
bool brd_io_is_charging(void) { return test_charging; }

static void sample(int voltage, bool charging = false) {
    test_battery_mv = voltage;
    test_charging = charging;
    delay(1000);
    brd_battery_update();
}
static void start(int voltage, bool charging = false) {
    test_battery_mv = voltage;
    test_charging = charging;
    brd_battery_begin();
}
static void test_discharge_curve() {
    start(4007);
    assert(brd_battery_get_percent() == 90U);
    sample(3721);
    assert(brd_battery_get_percent() == 50U);
    sample(3505);
    assert(brd_battery_get_percent() == 10U);
    sample(4150);
    assert(brd_battery_get_percent() == 100U); /* 沒有 FULL 判斷，放電電壓可顯示 100% */
    start(3490);
    assert(brd_battery_get_percent() <= BATTERY_BOOT_MIN_PERCENT);
    assert(brd_battery_is_low_locked()); /* V1.2 原開機低電規則 */
    start(3700);
    sample(3470);
    assert(brd_battery_get_percent() < BATTERY_LOW_STOP_PERCENT);
    assert(brd_battery_is_low_locked()); /* V1.2 原運行中 <5% */
}
static void test_charging_compensation() {
    static_assert(BATTERY_CHARGE_SOC_RISE_INTERVAL_MS == 72000UL,
                  "200mAh / 100mA should take 72 seconds per percent.");
    start(3721);
    assert(brd_battery_get_percent() == 50U);
    sample(4070, true);
    assert(brd_battery_get_percent() == 50U); /* 插線不跳升 */
    for (int i = 1; i < 72; ++i) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 50U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
    sample(4070, false);
    assert(brd_battery_get_percent() == 51U); /* CHRG_DET 1s 暫時無效不跳值 */
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
    for (int i = 0; i < 4; ++i) { sample(3976, false); }
    assert(brd_battery_get_percent() == 51U); /* 拔線端電壓偏高也不能上跳 */
    sample(3721, false);
    assert(brd_battery_get_percent() == 50U); /* 放電立即降低 */
}
static void test_no_full_and_slow_100() {
    start(4034);
    assert(brd_battery_get_percent() == 95U);
    sample(4200, true);
    assert(brd_battery_get_percent() == 95U);
    for (int p = 96; p <= 100; p++) {
        for (int t = 0; t < 72; t++) { sample(4200, true); }
        assert(brd_battery_get_percent() == p);
    }
    /* [V1.2.1] 100% 由限速及電壓曲線達成；完全沒有 FULL 狀態機。 */
    sample(4200, true);
    assert(brd_battery_get_percent() == 100U);
    start(4200, true);
    assert(brd_battery_get_percent() == 100U); /* 開機充電沒有先前 SOC 基準 */
}
int main() {
    test_discharge_curve();
    test_charging_compensation();
    test_no_full_and_slow_100();
    puts("PASS: 21-point discharge SOC, V1.2 low-lock, 1%/72s charge, no FULL.");
}
