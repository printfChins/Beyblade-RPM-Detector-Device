/*
    檔案位置: BRD_OLED_V0.10/brd_battery.cpp
    [V0.10 恢復] 沿用附件 V1.9 的 ADC 直接換算、校正倍率與 Li-Po SOC 表。
    GPIO0 / A0 經 470k / 470k 分壓，電池 mV = ADC mV * 2。
    [保留] 單次取樣，不做 dummy conversion、平均或 IIR 濾波。
    [V0.10 修改] 電池資料由本模組保存，不依賴 BLE 或舊版 brd_context。
    [V0.10 新增] 低於 5% 鎖定；到達 10% 後交由主程式重新啟動設備。
*/
#include <driver/gpio.h>

#include "brd_battery.h"
#include "brd_config.h"

struct battery_soc_point_t {
    uint16_t voltage_mv;
    uint8_t percent;
};

/* [保留] V1.9 的靜置電壓電量估算表，用於 OLED 圖示。 */
static const battery_soc_point_t g_battery_soc_table[] = {
    {3300U,   0U},
    {3500U,   5U},
    {3600U,  10U},
    {3700U,  20U},
    {3750U,  30U},
    {3800U,  40U},
    {3850U,  50U},
    {3900U,  60U},
    {3950U,  70U},
    {4000U,  80U},
    {4050U,  85U},
    {4100U,  90U},
    {4150U,  95U},
    {4200U, 100U}
};

static bool g_battery_initialized = false;
static uint32_t g_last_battery_sample_ms = 0UL;
static uint16_t g_battery_voltage_mv = 0U;
static uint8_t g_battery_percent = 0U;
static bool g_battery_low_locked = false;

static uint16_t battery_read_voltage_mv(void) {
    /* [保留] 每個取樣週期只有這一次 ADC 讀取。 */
    uint32_t adc_mv = analogReadMilliVolts(BATTERY_ADC_GPIO);
    uint64_t battery_mv = (uint64_t)adc_mv *
        ((uint64_t)BATTERY_DIVIDER_R_TOP_OHM + BATTERY_DIVIDER_R_BOTTOM_OHM);
    battery_mv /= BATTERY_DIVIDER_R_BOTTOM_OHM;
    battery_mv *= BATTERY_CALIBRATION_NUMERATOR;
    battery_mv /= BATTERY_CALIBRATION_DENOMINATOR;

    /* [V0.10 保留] ADC 完成腳位初始化後，亦明確維持 GPIO0 無內部上下拉。 */
    (void)gpio_set_pull_mode((gpio_num_t)BATTERY_ADC_GPIO, GPIO_FLOATING);

    if (battery_mv > 65535ULL) {
        battery_mv = 65535ULL;
    }
    return (uint16_t)battery_mv;
}

static uint8_t battery_voltage_to_percent(uint16_t voltage_mv) {
    const size_t point_count = sizeof(g_battery_soc_table) / sizeof(g_battery_soc_table[0]);

    if (voltage_mv <= BATTERY_MIN_MV) {
        return 0U;
    }
    if (voltage_mv >= BATTERY_MAX_MV) {
        return 100U;
    }

    for (size_t index = 1U; index < point_count; index++) {
        const battery_soc_point_t &low = g_battery_soc_table[index - 1U];
        const battery_soc_point_t &high = g_battery_soc_table[index];
        if (voltage_mv <= high.voltage_mv) {
            uint32_t voltage_span = (uint32_t)high.voltage_mv - low.voltage_mv;
            uint32_t voltage_offset = (uint32_t)voltage_mv - low.voltage_mv;
            uint32_t percent_span = (uint32_t)high.percent - low.percent;
            uint32_t percent = (uint32_t)low.percent +
                ((voltage_offset * percent_span) + (voltage_span / 2U)) / voltage_span;
            return (uint8_t)(percent > 100U ? 100U : percent);
        }
    }
    return 100U;
}

void brd_battery_begin(void) {
    /* [恢復] GPIO0 使用 12-bit ADC 與原版 11 dB attenuation。 */
    analogReadResolution(BATTERY_ADC_RESOLUTION_BITS);
    analogSetPinAttenuation(BATTERY_ADC_GPIO, BATTERY_ADC_ATTENUATION);
    (void)gpio_set_pull_mode((gpio_num_t)BATTERY_ADC_GPIO, GPIO_FLOATING);

    g_battery_initialized = false;
    g_battery_voltage_mv = 0U;
    g_battery_percent = 0U;
    g_last_battery_sample_ms = 0UL;
    g_battery_low_locked = false;
    brd_battery_update();
}

void brd_battery_update(void) {
    uint32_t now_ms = millis();
    if (g_battery_initialized &&
        (uint32_t)(now_ms - g_last_battery_sample_ms) < BATTERY_SAMPLE_INTERVAL_MS) {
        return;
    }

    g_last_battery_sample_ms = now_ms;
    g_battery_voltage_mv = battery_read_voltage_mv();
    g_battery_percent = battery_voltage_to_percent(g_battery_voltage_mv);
    g_battery_initialized = true;

    /* [V0.10 新增] 鎖定後不因回升到 5% 到 9% 而解除，也不直接恢復量測。 */
    if (g_battery_percent < BATTERY_LOW_STOP_PERCENT) {
        g_battery_low_locked = true;
    }
}

uint16_t brd_battery_get_voltage_mv(void) {
    return g_battery_voltage_mv;
}

uint8_t brd_battery_get_percent(void) {
    return g_battery_percent;
}

bool brd_battery_is_low_locked(void) {
    return g_battery_low_locked;
}

bool brd_battery_restart_required(void) {
    return g_battery_initialized && g_battery_low_locked &&
        g_battery_percent >= BATTERY_RECOVER_PERCENT;
}
