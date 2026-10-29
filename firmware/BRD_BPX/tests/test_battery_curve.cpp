/*
    [V1.4 充電 SOC 修改] 使用實際 brd_battery.cpp 及模擬 ADC/CHRG_DET 測試。
    檔案: BRD_BPX/tests/test_battery_curve.cpp
    [V1.4 本次修改] 200mAh／100mA 時每 72 秒最多 +1%；保留插線不跳升、無補跳。
    [保留] 3.46V 低電保護、3.42V 緊急保護、3.65V 恢復、99% 與 FULL。
*/
#include <Arduino.h>
#include <cassert>
#include <cstdio>

#include "brd_battery.h"
#include "brd_config.h"

uint64_t test_clock_us = 100000;
void (*test_interrupts[32])(void) = {};
int test_battery_mv = 4000;
bool test_adc_fail = false;
bool test_charging = false;

bool brd_io_is_charging(void) {
    return test_charging;
}

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

static void test_curve_and_low_lock(void) {
    start(4000);
    assert(brd_battery_get_percent() == 89U || brd_battery_get_percent() == 90U);
    assert(!brd_battery_is_low_locked());
    sample(4007);
    assert(brd_battery_get_percent() == 90U);
    sample(3721);
    assert(brd_battery_get_percent() == 50U);
    sample(3505);
    assert(brd_battery_get_percent() == 10U);
    sample(3462);
    assert(brd_battery_get_percent() >= 1U);
    sample(3460);
    assert(!brd_battery_is_low_locked());
    sample(3470);
    assert(!brd_battery_is_low_locked());
    sample(3460);
    sample(3460);
    assert(!brd_battery_is_low_locked());
    sample(3460);
    assert(brd_battery_is_low_locked());
    assert(brd_battery_get_percent() == 0U);

    for (int i = 0; i < 60; i++) {
        sample(3700, false);
    }
    assert(!brd_battery_restart_required());
    for (int i = 0; i < 30; i++) {
        sample(3650, true);
    }
    assert(!brd_battery_restart_required());
    sample(3650, true);
    assert(brd_battery_restart_required());
    start(3700, false);
    assert(!brd_battery_is_low_locked());
    sample(3420, false);
    assert(brd_battery_is_low_locked());
    assert(brd_battery_get_percent() == 0U);
    start(3420, false);
    assert(brd_battery_is_low_locked());
}

static void test_charge_soc_no_jump(void) {
    /* [本次新增] 200mAh / 100mA，每增加 1% 理想所需 72,000ms。 */
    static_assert(BATTERY_CHARGE_SOC_RISE_INTERVAL_MS == 72000UL,
                  "200mAh at 100mA must take 72 seconds per SOC percent.");
    const uint32_t steps_per_percent = BATTERY_CHARGE_SOC_RISE_INTERVAL_MS /
        BATTERY_SAMPLE_INTERVAL_MS;

    start(3721, false);
    assert(brd_battery_get_percent() == 50U);
    /* [保留] 插線立刻從 3.721V 抬到約 4.07V，不得立刻上拉。 */
    sample(4070, true);
    assert(brd_battery_get_percent() == 50U);
    for (uint32_t i = 1U; i < steps_per_percent; ++i) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 50U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
    for (uint32_t i = 0U; i < steps_per_percent; ++i) {
        sample(4070, true);
    }
    assert(brd_battery_get_percent() == 52U);

    /* [保留] 原始 SOC 下降時不借用已經累計的時間補跳。 */
    sample(3650, true);
    assert(brd_battery_get_percent() == 52U);
    sample(4070, true);
    assert(brd_battery_get_percent() == 52U);
    for (uint32_t i = 2U; i < steps_per_percent; ++i) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 52U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 53U);

    /* [保留] CHRG_DET 短暫 HIGH 不可以直接按充電高電壓跳升。 */
    sample(4070, false);
    assert(brd_battery_get_percent() == 53U);
    sample(4070, true);
    assert(brd_battery_get_percent() == 53U);
    /* [保留] 正式拔線後，高電壓不能使 SOC 瞬間上跳。 */
    for (int i = 0; i < 4; i++) {
        sample(3970, false);
    }
    assert(brd_battery_get_percent() == 53U);
    /* [保留] 放電曲線小於顯示值時可以立即下修，不延遲低電保護。 */
    sample(3700, false);
    assert(brd_battery_get_percent() <= 53U);
}

static void test_charge_soc_99_cap(void) {
    /* [本次新增] 低於 4.15V 時即使時間足夠，也只能以每 72 秒增加 1% 至 99%。 */
    start(4007, false);
    assert(brd_battery_get_percent() == 90U);
    sample(4149, true);
    assert(brd_battery_get_percent() == 90U);
    const uint32_t steps_per_percent = BATTERY_CHARGE_SOC_RISE_INTERVAL_MS /
        BATTERY_SAMPLE_INTERVAL_MS;
    for (uint32_t percent = 91U; percent <= 99U; ++percent) {
        for (uint32_t i = 1U; i < steps_per_percent; ++i) {
            sample(4149, true);
            assert(brd_battery_get_percent() == percent - 1U);
        }
        sample(4149, true);
        assert(brd_battery_get_percent() == percent);
    }
    for (uint32_t i = 0U; i < steps_per_percent * 2U; ++i) {
        sample(4149, true);
    }
    assert(brd_battery_get_percent() == 99U);
    assert(!brd_battery_is_full());
}

static void test_full_and_unplug(void) {
    start(3800, false);
    assert(!brd_battery_is_full());
    uint8_t before = brd_battery_get_percent();
    sample(4200, true);
    /* [新增] FULL 30 秒穩定計時完成前不能利用高電壓直接跳到 99%。 */
    assert(brd_battery_get_percent() == before);
    for (int i = 0; i < 29; i++) {
        sample(4200, true);
        assert(!brd_battery_is_full());
        assert(brd_battery_get_percent() == before);
    }
    sample(4200, true);
    assert(brd_battery_is_full());
    assert(brd_battery_get_percent() == 100U);
    sample(4130, false);
    assert(brd_battery_is_full());
    assert(brd_battery_get_percent() == 100U);
    sample(4100, false);
    assert(!brd_battery_is_full());
    assert(brd_battery_get_percent() < 100U);
}

static void test_charge_at_boot_and_99_cap(void) {
    /* [保留] 開機已插充電線，沒有上一筆百分比，首次 ADC 作基準。 */
    start(4200, true);
    assert(!brd_battery_is_full());
    assert(brd_battery_get_percent() == 99U);
    for (int i = 0; i < 30; i++) {
        sample(4200, true);
    }
    assert(brd_battery_is_full());
    assert(brd_battery_get_percent() == 100U);
}

int main() {
    test_curve_and_low_lock();
    test_charge_soc_no_jump();
    test_charge_soc_99_cap();
    test_full_and_unplug();
    test_charge_at_boot_and_99_cap();
    puts("PASS: ME6211 100mA SOC, 3.46V lock, charging no jump, 1%/72s, 99%/FULL.");
}
