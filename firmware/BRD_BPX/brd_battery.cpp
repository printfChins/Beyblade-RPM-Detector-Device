/*
    [BRD_BBP 修改] 僅更新檔案路徑註解，原功能保留。
    檔案位置: BRD_BPX/brd_battery.cpp
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
#include "brd_io.h" /* [V1.4 新增] 讀取充電中 GPIO10 狀態 */

struct battery_soc_point_t {
    uint16_t voltage_mv;
    uint8_t percent;
};

/*
    [V1.4 ME6211/100mA 修改] 取消原 3.37V=0% 的 21 點表。
    [新增] 以 CSV 約 100mA 定電流放電實測容量換算的 21 點電壓-SOC 表。
    使用 21 點移動中位數降低原始 CSV 測量跳動，每 5% 放電容量取電壓。
    0% = 3.46V：ME6211C33(100mA) 典型壓差 0.12V，3.30+0.12=3.42V。
    距離 3.42V 約 7.82 分鐘；此為典型壓差參考，非 LDO 最差規格保證。
    [保留] 整數線性插值 O(21)；ADC 每 1s 最多取樣一次，不碰 RPM ISR。
*/
static const battery_soc_point_t g_battery_soc_table[] = {
    {3460U,   0U}, {3484U,   5U}, {3505U,  10U}, {3525U,  15U},
    {3546U,  20U}, {3568U,  25U}, {3591U,  30U}, {3619U,  35U},
    {3651U,  40U}, {3685U,  45U}, {3721U,  50U}, {3757U,  55U},
    {3790U,  60U}, {3823U,  65U}, {3857U,  70U}, {3885U,  75U},
    {3944U,  80U}, {3976U,  85U}, {4007U,  90U}, {4034U,  95U},
    {4150U, 100U}
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
/* [V1.4 修改] 低電鎖定不再以百分比或「開機／運轉」不同門檻判斷。 */
static bool g_battery_startup_check_pending = true;
static bool g_battery_low_candidate_active = false;
static uint32_t g_battery_low_candidate_start_ms = 0UL;
static bool g_battery_charging_seen_locked = false;
static bool g_battery_full_latched = false;
static bool g_battery_full_candidate_active = false;
static uint32_t g_battery_full_candidate_start_ms = 0UL;
/* [V1.4 本次修改] 充電顯示增長時間改由 200mAh／100mA 推導為每 72 秒 +1%。
   [保留] RAM 記錄上次顯示電量與充電區間的計時，不動 RPM、OLED 與 BLE 流程。
   [刪減] 充電狀態下每秒以端電壓直接覆寫顯示百分比的作法。 */
static bool g_battery_charge_soc_tracking = false;
static uint32_t g_battery_charge_soc_last_step_ms = 0UL;
/* [V1.4 充電 SOC 新增] CHRG_DET 抖動時不將高電壓誤作放電 SOC，拔線後也不向上跳值。 */
static bool g_battery_charge_det_exit_active = false;
static uint32_t g_battery_charge_det_exit_start_ms = 0UL;
static bool g_battery_charge_soc_reconcile_after_unplug = false;
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
    /* [V1.4 新增] 有效連續取樣中斷，低電/FULL 的穩定計時重新開始。 */
    g_battery_low_candidate_active = false;
    g_battery_full_candidate_active = false;
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
    /* [V1.4 ME6211/100mA 修改] 每次開機以 3.46V 作 SOC 0% 與警示門檻。 */
    g_battery_startup_check_pending = true;
    g_battery_low_candidate_active = false;
    g_battery_low_candidate_start_ms = 0UL;
    g_battery_charging_seen_locked = false;
    g_battery_full_latched = false;
    g_battery_full_candidate_active = false;
    g_battery_full_candidate_start_ms = 0UL;
    /* [V1.4 充電 SOC 新增] 重啟時重置 RAM 內充電進度計時；首次有效 ADC 以當時電壓建立基準。 */
    g_battery_charge_soc_tracking = false;
    g_battery_charge_soc_last_step_ms = 0UL;
    g_battery_charge_det_exit_active = false;
    g_battery_charge_det_exit_start_ms = 0UL;
    g_battery_charge_soc_reconcile_after_unplug = false;
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
    /* [V1.4 充電 SOC 新增] 先保留上一筆顯示值；新 ADC 讀值僅作充電增長上限。 */
    const bool had_valid_sample = g_battery_diagnostics.has_valid_sample;
    const uint8_t previous_percent = g_battery_percent;
    const bool sample_gap_too_long = !had_valid_sample ||
        (uint32_t)(now_ms - g_battery_diagnostics.last_success_ms) > BATTERY_RECOVER_MAX_GAP_MS;
    if (sample_gap_too_long) {
        battery_reset_recovery();
        /* [V1.4 新增] 取樣過久不可把未驗證的時間算進低電或 FULL 連續達標。 */
        g_battery_low_candidate_active = false;
        g_battery_full_candidate_active = false;
    }
    g_battery_voltage_mv = (uint16_t)battery_mv;
    const uint8_t voltage_based_percent = battery_voltage_to_percent(g_battery_voltage_mv);
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

    /*
        [V1.4 新增] GPIO10 LOW=充電中。>=4150mV 每筆連續達標 30s 才是 FULL。
        FULL 狀態保留到電壓 <=4100mV；避免充電中量測雜訊讓閃電反覆跳動。
        [刪減] 單純依電壓表到頂就宣布已充滿的邏輯。
    */
    const bool charging = brd_io_is_charging();
    if (g_battery_full_latched && g_battery_voltage_mv <= BATTERY_FULL_RELEASE_MV) {
        g_battery_full_latched = false;
    }
    if (!g_battery_full_latched && charging &&
        g_battery_voltage_mv >= BATTERY_FULL_CONFIRM_MV) {
        if (!g_battery_full_candidate_active) {
            g_battery_full_candidate_active = true;
            g_battery_full_candidate_start_ms = now_ms;
        } else if ((uint32_t)(now_ms - g_battery_full_candidate_start_ms) >= BATTERY_FULL_CONFIRM_MS) {
            g_battery_full_latched = true;
        }
    } else {
        g_battery_full_candidate_active = false;
    }
    /*
        [V1.4 充電 SOC 修改] 電壓仍每 1 秒取樣，只有顯示 SOC 作速率限制：
        - 插線（由非充電轉為充電）立即保留上一筆顯示值，不因充電電壓跳升而上拉。
        - [本次修改] 200mAh／100mA 理想 CC 速率：每 72 秒最多增加 1%；
          以 BATTERY_CHARGE_SOC_RISE_INTERVAL_MS 參數換算，不再沿用每 60 秒 +1%。
          現有電壓曲線只作上限；不把充電端電壓當成實際 SOC。
        - 若電壓曲線 SOC 未超過顯示值，維持目前百分比並重置增長計時，避免累積補跳。
        - ADC 取樣中斷超過容許間隔，不補算遺失時間。
        - CHRG_DET 短暫去有效不立即切回放電曲線，避免充電端電壓誤判跳升。
        - 拔線後回到放電曲線，但只准立即向下修正；向上修正沿用相同的 72 秒限速。
        - 不延遲任何低電、緊急關機、充電恢復、FULL 電壓判定。
        [保留] 未 FULL 時顯示上限 99%，確認 FULL 才顯示 100%。
    */
    const uint8_t capped_voltage_percent =
        voltage_based_percent >= 100U ? 99U : voltage_based_percent;
    if (charging) {
        g_battery_charge_det_exit_active = false;
        g_battery_charge_soc_reconcile_after_unplug = true;
        if (!g_battery_charge_soc_tracking || sample_gap_too_long) {
            g_battery_charge_soc_tracking = true;
            g_battery_charge_soc_last_step_ms = now_ms;
        }
        if (!had_valid_sample) {
            /* [保留] 冷開機時沒有插線前的百分比，不從 0% 假裝開始充電。 */
            g_battery_percent = capped_voltage_percent;
        } else {
            uint8_t stable_percent = previous_percent >= 100U ? 99U : previous_percent;
            if (capped_voltage_percent > stable_percent &&
                (uint32_t)(now_ms - g_battery_charge_soc_last_step_ms) >=
                    BATTERY_CHARGE_SOC_RISE_INTERVAL_MS) {
                stable_percent++;
                g_battery_charge_soc_last_step_ms = now_ms;
            } else if (capped_voltage_percent <= stable_percent) {
                /* [新增] 只有目標高於目前顯示值時才允許累積加 1% 的時間。 */
                g_battery_charge_soc_last_step_ms = now_ms;
            }
            g_battery_percent = stable_percent;
        }
    } else {
        /* [新增] 充電偵測 HIGH 短脈衝不重新按充電高電壓推算顯示值。 */
        if (g_battery_charge_soc_tracking && !g_battery_charge_det_exit_active) {
            g_battery_charge_det_exit_active = true;
            g_battery_charge_det_exit_start_ms = now_ms;
        }
        if (g_battery_charge_soc_tracking &&
            (uint32_t)(now_ms - g_battery_charge_det_exit_start_ms) <
                BATTERY_CHARGE_DET_EXIT_CONFIRM_MS) {
            /* [新增] 3 秒內暫時沿用上一筆百分比；FULL 解除時也必須立即降至 99%。 */
            g_battery_percent = g_battery_full_latched ? previous_percent :
                (previous_percent >= 100U ? 99U : previous_percent);
        } else {
            g_battery_charge_soc_tracking = false;
            g_battery_charge_det_exit_active = false;
            if (g_battery_charge_soc_reconcile_after_unplug && had_valid_sample &&
                !g_battery_full_latched && capped_voltage_percent > previous_percent) {
                /* [新增] 剛拔充電線時，電壓偏高只允許顯示慢速向上回歸。 */
                if (sample_gap_too_long) {
                    g_battery_charge_soc_last_step_ms = now_ms;
                }
                g_battery_percent = previous_percent;
                if ((uint32_t)(now_ms - g_battery_charge_soc_last_step_ms) >=
                    BATTERY_CHARGE_SOC_RISE_INTERVAL_MS) {
                    g_battery_percent++;
                    g_battery_charge_soc_last_step_ms = now_ms;
                }
            } else {
                /* [保留] 自然放電可立即下降；沒有充電歷史的量測保持舊行為。 */
                g_battery_percent = capped_voltage_percent;
                g_battery_charge_soc_reconcile_after_unplug = false;
            }
        }
    }
    if (g_battery_full_latched) {
        /* [保留] 充電中 >=4.15V 連續 30 秒後 FULL/100%，優先於充電速率限制。 */
        g_battery_percent = 100U;
    }
    /* [V1.4 ME6211/100mA 修改] 3460mV 以上至少顯示 1%，0% 僅保留給電壓截止／警告。 */
    if (!g_battery_full_latched && g_battery_percent == 0U &&
        g_battery_voltage_mv > BATTERY_LOW_LOCK_MV) {
        g_battery_percent = 1U;
    }

    /*
        [V1.4 ME6211/100mA 修改] LDO 輸出 3.3V；100mA 典型 dropout 0.12V。
        電池 3.46V 警示（相對 3.42V 典型壓差點提前約 7.82 分鐘）。
        開機 <=3460mV 立即鎖定；運行中連續 2s <=3460mV 鎖定；
        <=3420mV 立即鎖定。3.42V 不是 LDO 保證穩壓的絕對底限。
        [保留] 鎖定後只能重新開機，不能直接開啟 RPM ISR。
    */
    if (g_battery_startup_check_pending) {
        g_battery_startup_check_pending = false;
        if (g_battery_voltage_mv <= BATTERY_LOW_LOCK_MV) {
            g_battery_low_locked = true;
        }
    }
    if (!g_battery_low_locked) {
        if (g_battery_voltage_mv <= BATTERY_LOW_HARD_STOP_MV) {
            g_battery_low_locked = true;
        } else if (g_battery_voltage_mv <= BATTERY_LOW_LOCK_MV) {
            if (!g_battery_low_candidate_active) {
                g_battery_low_candidate_active = true;
                g_battery_low_candidate_start_ms = now_ms;
            } else if ((uint32_t)(now_ms - g_battery_low_candidate_start_ms) >= BATTERY_LOW_CONFIRM_MS) {
                g_battery_low_locked = true;
            }
        } else {
            g_battery_low_candidate_active = false;
        }
    }
    if (g_battery_low_locked) {
        /* [V1.4 新增] 警告畫面固定視為 0%；拒絕因負載下降造成的假電量回升。 */
        g_battery_percent = 0U;
        if (charging) {
            g_battery_charging_seen_locked = true;
        }
    }

    /*
        [V1.4 修改] 充電確實發生過，且 >=3650mV 連續 30s 才解除保護並重啟。
        [刪減] 舊版 10% 且 3s 的恢復邏輯；消除負載卸除後電壓回彈的重開機循環。
    */
    bool recovery_allowed = g_battery_low_locked && g_battery_charging_seen_locked &&
        g_battery_voltage_mv >= BATTERY_RECOVER_MV;
    if (!recovery_allowed) {
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

/* [V1.4 新增] OLED 是否熄滅充電閃電，以及 100% 的唯一真實依據。 */
bool brd_battery_is_full(void) {
    return g_battery_full_latched;
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
