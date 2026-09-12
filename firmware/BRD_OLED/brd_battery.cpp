/*
    檔案位置: BRD_OLED/brd_battery.cpp
    [V0.12 刪減] analogReadMilliVolts() 無法區分錯誤回傳與有效 0 mV 的路徑。
    [V0.12 新增] ESP-IDF ADC oneshot 讀值、校正與明確 esp_err_t 診斷。
    [保留] 每秒最多單次 ADC 轉換，直接換算 470k / 470k 分壓與 SOC。
    不做 dummy conversion、平均、IIR 或相鄰 ADC 樣本濾波。
    [V0.12 新增] ADC 有效資料逾時暫停量測；低電恢復需連續達標 3 秒。
    [保留] 開機清除低電鎖定，未新增跨重啟保存。
*/
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#include "brd_battery.h"
#include "brd_config.h"

struct battery_soc_point_t {
    uint16_t voltage_mv;
    uint8_t percent;
};

/* [保留] 原版 SOC 表與整數百分比四捨五入方式。 */
static const battery_soc_point_t g_battery_soc_table[] = {
    {3300U,   0U}, {3500U,   5U}, {3600U,  10U}, {3700U,  20U},
    {3750U,  30U}, {3800U,  40U}, {3850U,  50U}, {3900U,  60U},
    {3950U,  70U}, {4000U,  80U}, {4050U,  85U}, {4100U,  90U},
    {4150U,  95U}, {4200U, 100U}
};

static adc_oneshot_unit_handle_t g_battery_adc_unit = nullptr;
static adc_cali_handle_t g_battery_adc_calibration = nullptr;
static adc_unit_t g_battery_unit_id = ADC_UNIT_1;
static adc_channel_t g_battery_adc_channel = ADC_CHANNEL_0;
static bool g_battery_channel_ready = false;
static bool g_battery_attempted = false;
/* [V0.12 新增] 逾時故障保持到下一筆成功讀值，避免長時間故障跨 millis 回繞。 */
static bool g_battery_adc_fault_active = true;
static uint16_t g_battery_voltage_mv = 0U;
static uint8_t g_battery_percent = 0U;
static bool g_battery_low_locked = false;
static bool g_battery_recovery_active = false;
static bool g_battery_recovery_ready = false;
static uint32_t g_battery_recovery_start_ms = 0UL;
static brd_battery_diagnostics_t g_battery_diagnostics = {};

static void battery_increment(uint32_t &value) {
    if (value != UINT32_MAX) {
        value++;
    }
}

static void battery_reset_recovery(void) {
    g_battery_recovery_active = false;
    g_battery_recovery_ready = false;
}

static void battery_record_failure(brd_battery_status_t status, esp_err_t error) {
    g_battery_diagnostics.status = status;
    g_battery_diagnostics.last_attempt_error = error;
    g_battery_diagnostics.last_failure_error = error;
    g_battery_diagnostics.last_failure_ms = millis();
    g_battery_diagnostics.last_sample_valid = false;
    battery_increment(g_battery_diagnostics.failure_count);
    battery_increment(g_battery_diagnostics.consecutive_failures);
    battery_reset_recovery();
    /* [V0.12 新增] 不把失敗寫成 0% 或設定低電鎖定，保留最後有效電量。 */
}

static esp_err_t battery_prepare_adc(void) {
    esp_err_t error;
    if (g_battery_adc_unit == nullptr) {
        error = adc_oneshot_io_to_channel(BATTERY_ADC_GPIO, &g_battery_unit_id,
                                         &g_battery_adc_channel);
        if (error != ESP_OK) {
            return error;
        }
        if (g_battery_unit_id != ADC_UNIT_1) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        adc_oneshot_unit_init_cfg_t config = {};
        config.unit_id = g_battery_unit_id;
        config.ulp_mode = ADC_ULP_MODE_DISABLE;
        error = adc_oneshot_new_unit(&config, &g_battery_adc_unit);
        if (error != ESP_OK) {
            return error;
        }
    }
    if (!g_battery_channel_ready) {
        adc_oneshot_chan_cfg_t config = {};
        config.atten = BATTERY_ADC_ATTENUATION;
        config.bitwidth = (adc_bitwidth_t)BATTERY_ADC_RESOLUTION_BITS;
        error = adc_oneshot_config_channel(g_battery_adc_unit, g_battery_adc_channel, &config);
        if (error != ESP_OK) {
            return error;
        }
        error = gpio_set_pull_mode((gpio_num_t)BATTERY_ADC_GPIO, GPIO_FLOATING);
        if (error != ESP_OK) {
            return error;
        }
        g_battery_channel_ready = true;
    }
    return ESP_OK;
}

static esp_err_t battery_prepare_calibration(void) {
    if (g_battery_adc_calibration != nullptr) {
        return ESP_OK;
    }
    adc_cali_curve_fitting_config_t config = {};
    config.unit_id = g_battery_unit_id;
    config.chan = g_battery_adc_channel;
    config.atten = BATTERY_ADC_ATTENUATION;
    config.bitwidth = (adc_bitwidth_t)BATTERY_ADC_RESOLUTION_BITS;
    return adc_cali_create_scheme_curve_fitting(&config, &g_battery_adc_calibration);
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
    /* [保留] 真正 MCU 重啟後此鎖定為 false；重複 begin 不重複配置既有 ADC handle。 */
    g_battery_attempted = false;
    g_battery_adc_fault_active = true;
    g_battery_voltage_mv = 0U;
    g_battery_percent = 0U;
    g_battery_low_locked = false;
    g_battery_diagnostics = {};
    battery_reset_recovery();
    brd_battery_update();
}

void brd_battery_update(void) {
    uint32_t now_ms = millis();
    if (g_battery_attempted &&
        (uint32_t)(now_ms - g_battery_diagnostics.last_attempt_ms) < BATTERY_SAMPLE_INTERVAL_MS) {
        return;
    }
    g_battery_attempted = true;
    g_battery_diagnostics.last_attempt_ms = now_ms;

    esp_err_t error = battery_prepare_adc();
    if (error != ESP_OK) {
        battery_record_failure(BRD_BATTERY_INIT_ERROR, error);
        return;
    }
    error = battery_prepare_calibration();
    if (error != ESP_OK) {
        battery_record_failure(BRD_BATTERY_CALIBRATION_ERROR, error);
        return;
    }

    int raw = 0;
    /* [V0.12 修改] 每個排程週期最多呼叫一次 ADC 轉換，失敗留待下個週期。 */
    error = adc_oneshot_read(g_battery_adc_unit, g_battery_adc_channel, &raw);
    if (error != ESP_OK) {
        battery_record_failure(BRD_BATTERY_READ_ERROR, error);
        return;
    }
    int adc_mv = 0;
    error = adc_cali_raw_to_voltage(g_battery_adc_calibration, raw, &adc_mv);
    if (error != ESP_OK) {
        battery_record_failure(BRD_BATTERY_CALIBRATION_ERROR, error);
        return;
    }
    if (raw < 0 || raw > (int)((1UL << BATTERY_ADC_RESOLUTION_BITS) - 1UL) || adc_mv < 0) {
        battery_record_failure(BRD_BATTERY_DATA_ERROR, ESP_ERR_INVALID_RESPONSE);
        return;
    }

    uint64_t battery_mv = (uint64_t)adc_mv *
        ((uint64_t)BATTERY_DIVIDER_R_TOP_OHM + BATTERY_DIVIDER_R_BOTTOM_OHM);
    battery_mv /= BATTERY_DIVIDER_R_BOTTOM_OHM;
    battery_mv *= BATTERY_CALIBRATION_NUMERATOR;
    battery_mv /= BATTERY_CALIBRATION_DENOMINATOR;
    if (battery_mv > UINT16_MAX) {
        battery_record_failure(BRD_BATTERY_DATA_ERROR, ESP_ERR_INVALID_RESPONSE);
        return;
    }

    now_ms = millis();
    if (!g_battery_diagnostics.has_valid_sample ||
        (uint32_t)(now_ms - g_battery_diagnostics.last_success_ms) > BATTERY_RECOVER_MAX_GAP_MS) {
        battery_reset_recovery();
    }
    g_battery_voltage_mv = (uint16_t)battery_mv;
    g_battery_percent = battery_voltage_to_percent(g_battery_voltage_mv);
    g_battery_diagnostics.status = BRD_BATTERY_OK;
    g_battery_diagnostics.last_attempt_error = ESP_OK;
    g_battery_diagnostics.last_success_ms = now_ms;
    g_battery_diagnostics.last_valid_raw = raw;
    g_battery_diagnostics.last_valid_adc_mv = adc_mv;
    g_battery_diagnostics.last_sample_valid = true;
    g_battery_diagnostics.has_valid_sample = true;
    g_battery_adc_fault_active = false;
    g_battery_diagnostics.consecutive_failures = 0UL;
    battery_increment(g_battery_diagnostics.success_count);

    /* [保留] ESP_OK 的有效低電讀值立即鎖定，包括有效 0 mV；不做去抖或濾波。 */
    if (g_battery_percent < BATTERY_LOW_STOP_PERCENT) {
        g_battery_low_locked = true;
    }
    if (!g_battery_low_locked || g_battery_percent < BATTERY_RECOVER_PERCENT) {
        battery_reset_recovery();
    } else {
        if (!g_battery_recovery_active) {
            g_battery_recovery_active = true;
            g_battery_recovery_start_ms = now_ms;
        }
        g_battery_recovery_ready =
            (uint32_t)(now_ms - g_battery_recovery_start_ms) >= BATTERY_RECOVER_STABLE_MS;
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

bool brd_battery_is_adc_fault(void) {
    if (!g_battery_diagnostics.has_valid_sample ||
        (uint32_t)(millis() - g_battery_diagnostics.last_success_ms) >= BATTERY_ADC_STALE_TIMEOUT_MS) {
        g_battery_adc_fault_active = true;
    }
    return g_battery_adc_fault_active;
}

bool brd_battery_measurement_allowed(void) {
    return !g_battery_low_locked && !brd_battery_is_adc_fault();
}

bool brd_battery_restart_required(void) {
    return g_battery_low_locked && g_battery_recovery_ready &&
        g_battery_diagnostics.last_sample_valid && !brd_battery_is_adc_fault() &&
        (uint32_t)(millis() - g_battery_diagnostics.last_success_ms) <= BATTERY_RECOVER_MAX_GAP_MS;
}

brd_battery_diagnostics_t brd_battery_get_diagnostics(void) {
    brd_battery_diagnostics_t result = g_battery_diagnostics;
    result.adc_fault_active = brd_battery_is_adc_fault();
    result.recovery_pending = g_battery_recovery_active &&
        (uint32_t)(millis() - g_battery_diagnostics.last_success_ms) <= BATTERY_RECOVER_MAX_GAP_MS;
    /* [V0.12 新增] 回報已由有效樣本確認的持續時間，不使用未取樣的空白時間。 */
    result.recovery_elapsed_ms = result.recovery_pending ?
        (uint32_t)(g_battery_diagnostics.last_success_ms - g_battery_recovery_start_ms) : 0UL;
    return result;
}
