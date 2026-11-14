/*
    [本次修改] BRD_OLED 電量估算、充電補償與低電保護對齊 BRD_BPX(10)。
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
#include <nvs.h> /* [本次新增] 容量檔位 NVS 保存；只在檔位變化時寫入。 */

#include "brd_battery.h"
#include "brd_config.h"
#include "brd_io.h" /* [V1.2.1 新增] GPIO10 充電狀態僅用於 SOC 補償，不偵測 FULL。 */

struct battery_soc_point_t {
    uint16_t voltage_mv;
    uint8_t percent;
};

/*
    [V1.2.1 修改] 刪除 V1.2 原 14 點通用 SOC 表；移植 V1.4.1 依
    602030 / 100mA 放電量測建立的 21 點查表。整數線性插值保留。
    [重要] 不引入 V1.4 的 FULL 狀態機或電壓式低電鎖定；
    原 V1.2 百分比低電保護繼續使用新曲線換算出的 SOC。
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
/* [新增] 區分「開機 <= 10%」鎖定與運行中 < 5% 鎖定，避免 10% 回彈造成重啟循環。 */
/*
    [V1.2.1 新增] V1.4.1 充電百分比補償（無 FULL）：
    - 插入充電不直接按端電壓跳升 SOC。
    - 200mAh / 100mA 每 72 秒最多 +1%；端電壓 SOC 為上限。
    - 充電偵測短暫離開最多等待 3 秒，避免拔線電壓震盪跳值。
    - 拔線後可立即下降，向上回歸仍按 72 秒限速。
    [刪減] FULL 候選、FULL 鎖存、FULL API、99% 封頂及 4.15V/30 秒判定。
*/
static bool g_battery_charge_soc_tracking = false;
static uint32_t g_battery_charge_soc_last_step_ms = 0UL;
static bool g_battery_charge_det_exit_active = false;
static uint32_t g_battery_charge_det_exit_start_ms = 0UL;
static bool g_battery_charge_soc_reconcile_after_unplug = false;
/*
    [本次新增] 自動容量檔位狀態；NVS 儲存檔位不儲存每秒的 SOC。
    LOW->HIGH 表示充電終止 *或* 拔線，沒有插線偵測器不能直接認定 FULL。
    [本次修改] 降檔須充電 >=20 分鐘、>=4.20V 連續 60 秒，
    再經 GPIO10 退出去抖 3 秒且退出前顯示 <=90% 才允許。
*/
static uint16_t g_battery_capacity_mah = BATTERY_NOMINAL_CAPACITY_MAH;
static bool g_battery_capacity_charge_session = false;
static bool g_battery_capacity_learn_eligible = false;
static bool g_battery_capacity_changed_this_session = false;
static uint32_t g_battery_capacity_session_start_ms = 0UL;
static bool g_battery_capacity_99_active = false;
static uint32_t g_battery_capacity_99_since_ms = 0UL;
static bool g_battery_capacity_full_voltage_active = false;
static uint32_t g_battery_capacity_full_voltage_since_ms = 0UL;
/* [本次新增] 僅在 CHRG_DET=LOW 且有效 ADC >=4.20V 真正滿 60 秒後鎖存候選，
   避免用第一次 HIGH 的時間補足最後一秒造成提前降檔。 */
static bool g_battery_capacity_full_voltage_confirmed = false;
static bool g_battery_capacity_full_before_exit = false;
static uint8_t g_battery_capacity_percent_before_exit = 0U;
static bool g_battery_startup_check_pending = true;
static bool g_battery_startup_low_locked = false;
static bool g_battery_recovery_active = false;
static bool g_battery_recovery_ready = false;
static uint32_t g_battery_recovery_start_ms = 0UL;
static brd_battery_diagnostics_t g_battery_diagnostics = {};

/* [本次新增] 限定 NVS 中可接受的檔位，不採信損毀/舊版任意數值。 */
static bool battery_capacity_valid(uint16_t mah) {
    return mah == 200U || mah == 250U || mah == 300U;
}

static uint32_t battery_capacity_soc_interval_ms(void) {
    return (3600000UL * (uint32_t)g_battery_capacity_mah) /
        (100UL * BATTERY_CHARGE_CURRENT_MA);
}

static void battery_capacity_load(void) {
    g_battery_capacity_mah = BATTERY_NOMINAL_CAPACITY_MAH;
    nvs_handle_t handle;
    if (nvs_open("brd_battery", NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    uint16_t saved = 0U;
    if (nvs_get_u16(handle, "capacity", &saved) == ESP_OK && battery_capacity_valid(saved)) {
        g_battery_capacity_mah = saved;
    }
    nvs_close(handle);
}

static void battery_capacity_save(void) {
    /* [本次新增] 學習檔位才寫 Flash，避免每秒 ADC 取樣造成 NVS 磨耗。 */
    nvs_handle_t handle;
    if (nvs_open("brd_battery", NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_u16(handle, "capacity", g_battery_capacity_mah) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

static void battery_capacity_change(int step) {
    const int next = (int)g_battery_capacity_mah + step * (int)BATTERY_CAPACITY_STEP_MAH;
    if (next < (int)BATTERY_CAPACITY_MIN_MAH || next > (int)BATTERY_CAPACITY_MAX_MAH) {
        return;
    }
    g_battery_capacity_mah = (uint16_t)next;
    battery_capacity_save();
    g_battery_capacity_changed_this_session = true;
}

static void battery_capacity_reset_session(void) {
    g_battery_capacity_charge_session = false;
    g_battery_capacity_learn_eligible = false;
    g_battery_capacity_changed_this_session = false;
    g_battery_capacity_session_start_ms = 0UL;
    g_battery_capacity_99_active = false;
    g_battery_capacity_99_since_ms = 0UL;
    g_battery_capacity_full_voltage_active = false;
    g_battery_capacity_full_voltage_since_ms = 0UL;
    g_battery_capacity_full_voltage_confirmed = false;
    g_battery_capacity_full_before_exit = false;
    g_battery_capacity_percent_before_exit = 0U;
}

static void battery_capacity_track_charging(uint32_t now_ms, uint8_t previous_percent,
                                             uint8_t displayed_percent, bool gap_too_long,
                                             uint16_t voltage_mv) {
    if (!g_battery_capacity_charge_session) {
        /* [本次新增] 充電前已接近 100% 不做容量學習。 */
        battery_capacity_reset_session();
        g_battery_capacity_charge_session = true;
        g_battery_capacity_session_start_ms = now_ms;
        g_battery_capacity_learn_eligible =
            previous_percent <= BATTERY_CAPACITY_LEARN_START_SOC_MAX;
    }
    if (gap_too_long) {
        /* [本次新增] 缺樣中斷 99% 與滿電連續時間，不以舊資料推斷。 */
        g_battery_capacity_99_active = false;
        g_battery_capacity_full_voltage_active = false;
        g_battery_capacity_full_voltage_confirmed = false;
    }

    if (voltage_mv >= BATTERY_CAPACITY_FULL_VOLTAGE_MV) {
        if (!g_battery_capacity_full_voltage_active) {
            /* [本次修改] 首次 >=4.20V 從本次有效樣本開始計時。 */
            g_battery_capacity_full_voltage_active = true;
            g_battery_capacity_full_voltage_confirmed = false;
            g_battery_capacity_full_voltage_since_ms = now_ms;
        } else if ((uint32_t)(now_ms - g_battery_capacity_full_voltage_since_ms) >=
                        BATTERY_CAPACITY_FULL_STABLE_MS) {
            /* [本次新增] 60 秒必須在充電 LOW 狀態內滿足，不計入退出去抖時間。 */
            g_battery_capacity_full_voltage_confirmed = true;
        }
    } else {
        /* [本次修改] 任一有效樣本跌破 4.20V，重新累積完整 60 秒。 */
        g_battery_capacity_full_voltage_active = false;
        g_battery_capacity_full_voltage_confirmed = false;
    }

    if (displayed_percent == 99U && g_battery_capacity_learn_eligible &&
        !g_battery_capacity_changed_this_session) {
        if (!g_battery_capacity_99_active) {
            /* [本次新增] 必須是本次充電由 <99% 走到 99%，不能剛插線就 99%。 */
            if (previous_percent < 99U || gap_too_long) {
                /* [本次新增] ADC 中斷後必須重新累積完整 20 分鐘，不能沿用舊秒數。 */
                g_battery_capacity_99_active = true;
                g_battery_capacity_99_since_ms = now_ms;
            }
        } else if ((uint32_t)(now_ms - g_battery_capacity_99_since_ms) >=
                        BATTERY_CAPACITY_99_STALL_MS &&
                   (uint32_t)(now_ms - g_battery_capacity_session_start_ms) >=
                        BATTERY_CAPACITY_MIN_SESSION_MS &&
                   g_battery_capacity_mah < BATTERY_CAPACITY_MAX_MAH) {
            /* [本次新增] 99% 長時間仍為充電 LOW：每一充電週期最多上調 50mAh。 */
            battery_capacity_change(+1);
            g_battery_capacity_99_active = false;
            g_battery_charge_soc_last_step_ms = now_ms;
        }
    } else if (displayed_percent != 99U) {
        g_battery_capacity_99_active = false;
    }
}

static void battery_capacity_prepare_exit(uint32_t now_ms, uint8_t displayed_percent) {
    /* [本次修改] 將 LOW 期間已通過的 4.20V/60秒證據凍結，
       不把 GPIO10 退出後的 3 秒當作高電壓充電時間。
       降檔只依列出的條件；充電初始 SOC<=85% 限制仍保留給 99% 升檔使用。 */
    g_battery_capacity_full_before_exit =
        g_battery_capacity_charge_session &&
        !g_battery_capacity_changed_this_session &&
        (uint32_t)(now_ms - g_battery_capacity_session_start_ms) >=
            BATTERY_CAPACITY_MIN_SESSION_MS &&
        g_battery_capacity_full_voltage_confirmed;
    g_battery_capacity_percent_before_exit = displayed_percent;
}

static void battery_capacity_confirm_exit(void) {
    /*
        [本次修改] 確認 GPIO10 已退出充電至少 3 秒，且退出前具備
        20 分鐘充電 + 4.20V 連續 60 秒 + 顯示 <=90% 的證據才降一檔。
        [刪減] 退出後 4.12V 再檢查的非指定條件；仍無法完全區分拔線與充滿。
        [保留] 不以這個推定充滿事件強制將 OLED SOC 設為 100%。
    */
    if (g_battery_capacity_full_before_exit &&
        g_battery_capacity_percent_before_exit <= BATTERY_CAPACITY_DOWN_SOC_MAX &&
        g_battery_capacity_mah > BATTERY_CAPACITY_MIN_MAH) {
        battery_capacity_change(-1);
    }
    battery_capacity_reset_session();
}

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
    /* [新增] 每次真正開機都必須重新通過 > 10% 的第一筆有效 ADC 檢查。 */
    g_battery_startup_check_pending = true;
    g_battery_startup_low_locked = false;
    /* [V1.2.1 新增] 開機重置充電補償計時，避免跨重啟補算。 */
    g_battery_charge_soc_tracking = false;
    g_battery_charge_soc_last_step_ms = 0UL;
    g_battery_charge_det_exit_active = false;
    g_battery_charge_det_exit_start_ms = 0UL;
    g_battery_charge_soc_reconcile_after_unplug = false;
    /* [本次新增] 重新開機從 NVS 還原合法檔位，並清掉上次充電學習暫存。 */
    battery_capacity_load();
    battery_capacity_reset_session();
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
    /* [V1.2.1 修改] 放電使用 21 點電壓表；充電只對顯示 SOC 限速。 */
    const bool had_valid_sample = g_battery_diagnostics.has_valid_sample;
    const uint8_t previous_percent = g_battery_percent;
    const bool sample_gap_too_long = !had_valid_sample ||
        (uint32_t)(now_ms - g_battery_diagnostics.last_success_ms) > BATTERY_RECOVER_MAX_GAP_MS;
    if (sample_gap_too_long) {
        battery_reset_recovery();
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
        [V1.2.1 移植] 以充電電流 / 電池容量推算的理論上升速率補償顯示電量。
        不使用 FULL 判定；到達 100% 只能由時間限速、電壓 SOC 上限共同達成。
        ADC 仍每秒讀取一筆，錯誤樣本不修改 SOC。
        參考專案的百分比低電鎖定與恢復機制仍位於下方，不另加電壓式鎖定。
    */
    const bool charging = brd_io_is_charging();
    if (charging) {
        /* [本次新增] CHRG_DET 低電位代表新充電週期；短暫高電位去抖後恢復時
           不可重建學習會話，避免拔插雜訊誘發錯誤升降檔。 */
        g_battery_charge_det_exit_active = false;
        g_battery_charge_soc_reconcile_after_unplug = true;
        if (!g_battery_charge_soc_tracking || sample_gap_too_long) {
            g_battery_charge_soc_tracking = true;
            g_battery_charge_soc_last_step_ms = now_ms;
        }
        if (!had_valid_sample) {
            /* [保留] 開機已在充電，沒有先前 SOC，只能以首次有效 ADC 建立基準。 */
            g_battery_percent = voltage_based_percent;
        } else {
            uint8_t stable_percent = previous_percent;
            if (voltage_based_percent > stable_percent &&
                (uint32_t)(now_ms - g_battery_charge_soc_last_step_ms) >=
                    battery_capacity_soc_interval_ms()) {
                stable_percent++;
                g_battery_charge_soc_last_step_ms = now_ms;
            } else if (voltage_based_percent <= stable_percent) {
                /* [保留] 未高於顯示值時不累積可一次補跳的時間。 */
                g_battery_charge_soc_last_step_ms = now_ms;
            }
            g_battery_percent = stable_percent;
        }
        /* [本次新增] 在充電狀態更新後觀察 99% 與高電壓時間，只調整容量檔位。 */
        battery_capacity_track_charging(now_ms, previous_percent, g_battery_percent,
                                        sample_gap_too_long, g_battery_voltage_mv);
    } else {
        /* [本次新增] ADC 有效取樣曾中斷時不允許沿用先前的滿電證據。 */
        if (sample_gap_too_long) {
            g_battery_capacity_full_voltage_active = false;
            g_battery_capacity_full_voltage_confirmed = false;
            g_battery_capacity_full_before_exit = false;
        }
        if (g_battery_charge_soc_tracking && !g_battery_charge_det_exit_active) {
            g_battery_charge_det_exit_active = true;
            g_battery_charge_det_exit_start_ms = now_ms;
            battery_capacity_prepare_exit(now_ms, previous_percent);
        }
        if (g_battery_charge_soc_tracking &&
            (uint32_t)(now_ms - g_battery_charge_det_exit_start_ms) <
                BATTERY_CHARGE_DET_EXIT_CONFIRM_MS) {
            /* [保留] CHRG_DET 短暫跳變不可使充電高電壓立即抬升 SOC。 */
            g_battery_percent = previous_percent;
        } else {
            g_battery_charge_soc_tracking = false;
            g_battery_charge_det_exit_active = false;
            if (g_battery_capacity_charge_session) {
                /* [本次新增] 退出 3 秒確認後，先評估是否為高電壓的充滿事件。 */
                battery_capacity_confirm_exit();
            }
            if (g_battery_charge_soc_reconcile_after_unplug && had_valid_sample &&
                voltage_based_percent > previous_percent) {
                if (sample_gap_too_long) {
                    g_battery_charge_soc_last_step_ms = now_ms;
                }
                g_battery_percent = previous_percent;
                if ((uint32_t)(now_ms - g_battery_charge_soc_last_step_ms) >=
                    battery_capacity_soc_interval_ms()) {
                    g_battery_percent++;
                    g_battery_charge_soc_last_step_ms = now_ms;
                }
            } else {
                /* [保留] 自然放電可立即下降；沒有充電歷史則直接依電壓換算。 */
                g_battery_percent = voltage_based_percent;
                g_battery_charge_soc_reconcile_after_unplug = false;
            }
        }
    }

    /*
        [本次修改] 與 BRD_BPX(10) 相同，低電判定依顯示 SOC 百分比。
        開機首次有效 ADC：<= BATTERY_BOOT_MIN_PERCENT（8%）即鎖定。
        正常運行：< BATTERY_LOW_STOP_PERCENT（5%）即鎖定。
        [保留] 充電百分比限速狀態也影響低電判定；完全依參考專案設計。
        [不修改] OLED/RPM/LOAD/AUTO 的量測與顯示流程。
    */
    if (g_battery_startup_check_pending) {
        g_battery_startup_check_pending = false;
        if (g_battery_percent <= BATTERY_BOOT_MIN_PERCENT) {
            g_battery_low_locked = true;
            g_battery_startup_low_locked = true;
        }
    } else if (g_battery_percent < BATTERY_LOW_STOP_PERCENT) {
        g_battery_low_locked = true;
        g_battery_startup_low_locked = false;
    }

    /*
        [保留] 開機鎖定需 SOC > 8%，運行鎖定需 SOC >= 10%，
        持續 BATTERY_RECOVER_STABLE_MS（3 秒）才允許軟重啟。
    */
    bool recovery_below_threshold = g_battery_startup_low_locked ?
        (g_battery_percent <= BATTERY_BOOT_MIN_PERCENT) :
        (g_battery_percent < BATTERY_RECOVER_PERCENT);
    if (!g_battery_low_locked || recovery_below_threshold) {
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

/* [本次新增] 供診斷讀取自動選擇的容量；不更動 OLED 或主程式。 */
uint16_t brd_battery_get_capacity_mah(void) {
    return g_battery_capacity_mah;
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
