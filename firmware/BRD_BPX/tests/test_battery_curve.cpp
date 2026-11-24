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
#include "brd_settings.h"
#include "brd_charge_math.h"

/* [V1.4.3 新增] 電池測試提供生效中的容量設定，模擬 BLE SAVE 後切換。 */
static brd_settings::Values test_settings = {0U, 0U, 30U, 2000U, BATTERY_CAPACITY_200_MAH, OLED_MAX_HOLD_MS};
const brd_settings::Values &brd_settings_get(void) { return test_settings; }

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
static void start(int voltage, bool charging = false, uint16_t capacity_mah = BATTERY_CAPACITY_200_MAH) {
    test_settings.battery_capacity_mah = capacity_mah;
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
    static_assert(BATTERY_CHARGE_SOC_RISE_INTERVAL_MS(BATTERY_CAPACITY_200_MAH) == 72000UL,
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
/* [V1.4.4 新增] 250mAh / 100mA 僅在第 90 秒增加 1%。 */
static void test_250mah_charging_compensation() {
    static_assert(BATTERY_CHARGE_SOC_RISE_INTERVAL_MS(BATTERY_CAPACITY_250_MAH) == 90000UL,
                  "250mAh / 100mA must take 90 seconds per percent.");
    start(3721, false, BATTERY_CAPACITY_250_MAH);
    assert(brd_battery_get_percent() == 50U);
    sample(4070, true);
    for (int i = 1; i < 90; ++i) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 50U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
}
/* [V1.4.3 新增] 300mAh 必須每 108 秒才增加 1%，且保留電壓 SOC 上限。 */
static void test_300mah_charging_compensation() {
    static_assert(BATTERY_CHARGE_SOC_RISE_INTERVAL_MS(BATTERY_CAPACITY_300_MAH) == 108000UL,
                  "300mAh / 100mA should take 108 seconds per percent.");
    start(3721, false, BATTERY_CAPACITY_300_MAH);
    assert(brd_battery_get_percent() == 50U);
    sample(4070, true);
    for (int i = 1; i < 108; ++i) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 50U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
    for (int i = 0; i < 108; ++i) { sample(3721, true); }
    assert(brd_battery_get_percent() == 51U); /* 電壓 SOC 低於顯示值，不會再上升。 */
}
/* [V1.4.3 新增] 充電途中切換 200->300，應從 SAVE 生效的時刻重新計時。 */
static void test_charge_capacity_switch() {
    start(3721, false, BATTERY_CAPACITY_200_MAH);
    sample(4070, true);
    for (int i = 0; i < 60; ++i) { sample(4070, true); }
    assert(brd_battery_get_percent() == 50U);
    test_settings.battery_capacity_mah = BATTERY_CAPACITY_300_MAH;
    sample(4070, true); /* 檢測到新檔位，重設升幅計時起點。 */
    assert(brd_battery_get_percent() == 50U);
    for (int i = 0; i < 107; ++i) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 50U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
}

/* [V1.4.4 新增] 驗證兩檔的時間門檻，且到期仍須電壓 SOC 高於目前顯示。 */
static void test_large_capacity_charging(uint16_t capacity_mah, unsigned interval_seconds) {
    start(3721, false, capacity_mah);
    sample(4070, true);
    assert(brd_battery_get_percent() == 50U);
    for (unsigned second = 1U; second < interval_seconds; ++second) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 50U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
    for (unsigned second = 0U; second < interval_seconds; ++second) {
        sample(3721, true);
    }
    assert(brd_battery_get_percent() == 51U);
}

/* [V1.4.4 新增] 新檔位切換後要重算完整區間，即使舊計時已超過新檔門檻。 */
static void test_charge_switch_to_faster_profile() {
    start(3721, false, 1000U);
    sample(4070, true);
    for (unsigned second = 0U; second < 200U; ++second) {
        sample(4070, true);
    }
    assert(brd_battery_get_percent() == 50U);
    test_settings.battery_capacity_mah = 500U;
    sample(4070, true);
    assert(brd_battery_get_percent() == 50U);
    for (unsigned second = 1U; second < 180U; ++second) {
        sample(4070, true);
        assert(brd_battery_get_percent() == 50U);
    }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
}

/* [V1.5.2 新增] 自訂 mAh/mA 補償；電流與容量修改後均需重新計時。 */
static void test_custom_charge_settings() {
    static_assert(brd_charge_one_percent_ms(300U, 100U) == 108000U, "default charge interval");
    static_assert(brd_charge_one_percent_ms(325U, 125U) == 93600U, "custom charge interval");
    static_assert(brd_charge_one_percent_ms(5000U, 10U) == 18000000U, "64-bit intermediate");
    test_settings.charge_current_ma = 200U;
    start(3721, false, 200U);
    sample(4070, true);
    for (unsigned s = 1U; s < 36U; ++s) { sample(4070, true); assert(brd_battery_get_percent() == 50U); }
    sample(4070, true);
    assert(brd_battery_get_percent() == 51U);
    /* 從 200mA 切回 100mA，原累積時間不得直接跳過 72 秒。 */
    test_settings.charge_current_ma = 100U;
    sample(4070, true);
    for (unsigned s = 1U; s < 72U; ++s) { sample(4070, true); assert(brd_battery_get_percent() == 51U); }
    sample(4070, true);
    assert(brd_battery_get_percent() == 52U);
    test_settings.charge_current_ma = 100U;
}

int main() {
    test_custom_charge_settings();
    test_discharge_curve();
    test_charging_compensation();
    test_no_full_and_slow_100();
    test_250mah_charging_compensation();
    test_300mah_charging_compensation();
    test_charge_capacity_switch();
    test_large_capacity_charging(500U, 180U);
    test_clock_us = (static_cast<uint64_t>(UINT32_MAX) - 120000U) * 1000U;
    test_large_capacity_charging(1000U, 360U); /* 計時跨越 millis() 回繞。 */
    test_charge_switch_to_faster_profile();
    puts("PASS: discharge SOC, low-lock, 72/90/108/180/360s charge intervals, capacity switching and millis wrap.");
}
