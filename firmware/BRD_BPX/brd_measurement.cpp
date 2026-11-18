/*
    檔案位置: BRD_BPX/brd_measurement.cpp
    [修改] GPIO3 僅使用 FALLING 中斷；每兩次下降緣形成一次完整旋轉週期。
    [刪減] CHANGE/RISING、參考極性選擇、5 點中位數、起始延後確認與連續低值確認。
    [修改] 每筆非零原始週期直接換算即時 RPM 並持續更新 MAX，不做平滑或去噪。
    [新增] 量測期間只把原始週期寫入固定 RAM；HOLD 成立後才轉成最多 32 點 BLE 曲線。
    [保留] LOAD 去抖、AUTO 最低轉速資格、無脈衝歸零、MAX HOLD、低電中止及時間回繞。
    [注意] 原始模式不抑制雜訊；硬體假下降緣會直接反映於 RPM/MAX。
*/
#include <soc/gpio_struct.h>

#include "brd_config.h"
#include "brd_bbp.h"
#include "brd_measurement.h"
/* [新增] 四項參數讀取執行時設定，ISR 不存取設定模型。 */
#include "brd_settings.h"

struct input_edge_t {
    uint32_t time_us;
    int level;
};

static portMUX_TYPE g_input_mux = portMUX_INITIALIZER_UNLOCKED;
/* [修改] RPM ISR 只收時間戳記，不再讀腳位電位或記錄邊沿極性。 */
static volatile uint32_t g_rpm_queue[RPM_ISR_QUEUE_SIZE];
static volatile input_edge_t g_load_queue[LOAD_ISR_QUEUE_SIZE];
static volatile uint16_t g_rpm_head = 0U;
static volatile uint16_t g_rpm_tail = 0U;
static volatile uint16_t g_load_head = 0U;
static volatile uint16_t g_load_tail = 0U;
static volatile bool g_rpm_overflow = false;
static volatile bool g_load_overflow = false;
static volatile bool g_input_capture = false;
static volatile int g_isr_load_level = LOW;

/* [V0.12 新增] 固定 RAM 快照，不使用 heap，也不把陣列放在主迴圈堆疊。 */
static uint32_t g_rpm_batch[RPM_ISR_QUEUE_SIZE];
static input_edge_t g_load_batch[LOAD_ISR_QUEUE_SIZE];
static brd_measurement_diagnostics_t g_diagnostics = {};
static bool g_measurement_enabled = false;
/* [新增] BLE callback 只讀 critical-section 保護的修改窗口，不讀量測狀態機。 */
static bool g_modification_window = true;
static bool g_settings_paused = false;
static bool g_measurement_stopped = false;
static bool g_measurement_interrupts_attached = false;
static bool g_load_interrupt_attached = false;
static brd_state_t g_state = BRD_STATE_WAIT_LOAD;
static int g_load_raw = LOW;
static int g_load_stable = LOW;
static uint32_t g_load_candidate_us = 0UL;
/* [修改] Falling-to-Falling 唯一週期基準。 */
static bool g_period_valid = false;
static bool g_quiet_expired = true;
static uint32_t g_last_edge_us = 0UL;
static uint32_t g_last_activity_us = 0UL;
static uint32_t g_launch_us = 0UL;
static bool g_has_valid_rpm = false;
static uint16_t g_current_rpm = 0U;
static uint16_t g_max_rpm = 0U;
/*
    [新增] 保存每筆原始 Falling-to-Falling 週期（單位：us），絕不在取樣中抽點、排序、濾波。
    ESP32-C3 無 PSRAM：最多保存 16384 筆（64 KiB）；超量由診斷計數反映，
    MAX/即時 RPM 仍持續計算。32 點 BLE 格式限制只在停止採樣後處理。
*/
static uint32_t g_raw_periods[RPM_RAW_CAPTURE_CAPACITY] = {};
static uint32_t g_raw_period_count = 0U;
static uint32_t g_max_period_us = 0U;
/* [新增] MAX 的原始索引，供停止後 32 點曲線盡可能保留峰值。 */
static uint32_t g_max_period_index = UINT32_MAX;
/* [新增] AUTO 達到執行時最低 RPM 後保存資格，共用百分比結束。 */
static bool g_auto_launch_candidate = false;
/* [刪減] AUTO 不再保存一秒零轉速候選時間戳記。 */
static bool g_show_max = false;
static uint16_t g_display_max = 0U;
static uint32_t g_display_generation = 0UL;
static bool g_max_lock = false;
/*
    [V1.2.2 新增] LOAD 模式在發射完成後，要求 HOLD 結束後的真實 LOW 去抖。
    必須先確認解除裝載，再接受下一個 LOW -> HIGH；不得把舊 HIGH 當新裝載。
*/
static bool g_load_release_required = false;
static bool g_max_frame_seen = false;
static uint32_t g_max_lock_start_ms = 0UL;

static void increment_counter(uint32_t &value) {
    if (value != UINT32_MAX) {
        value++;
    }
}

static bool time_before(uint32_t first, uint32_t second) {
    /* [V0.12 新增] 活躍事件與期限相隔小於 2^31 us，支援 micros 回繞。 */
    return (int32_t)(first - second) < 0;
}

static bool measurement_uses_load(void) {
    return brd_settings_get().mode == BRD_MEASUREMENT_MODE_LOAD;
}

/* [新增] WAIT / READY / 完成後 HOLD 可修改；SPINNING 不可修改。 */
static void update_modification_window(void) {
    bool allowed = !g_measurement_enabled || g_state == BRD_STATE_WAIT_LOAD ||
                   g_state == BRD_STATE_LOADED_READY;
    portENTER_CRITICAL(&g_input_mux);
    g_modification_window = allowed;
    portEXIT_CRITICAL(&g_input_mux);
}

static void IRAM_ATTR rpm_ir_isr(void) {
    /* [修改] 唯一 FALLING ISR：只紀錄時間戳，不計算除法、不讀 GPIO、不處理 BLE。 */
    const uint32_t edge_us = micros();
    portENTER_CRITICAL_ISR(&g_input_mux);
    if (g_input_capture) {
        const uint16_t next = static_cast<uint16_t>((g_rpm_head + 1U) % RPM_ISR_QUEUE_SIZE);
        if (next != g_rpm_tail) {
            g_rpm_queue[g_rpm_head] = edge_us;
            g_rpm_head = next;
        } else {
            g_rpm_overflow = true;
        }
    }
    portEXIT_CRITICAL_ISR(&g_input_mux);
}

static void IRAM_ATTR load_ir_isr(void) {
    uint32_t edge_us = micros();
    int level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
    portENTER_CRITICAL_ISR(&g_input_mux);
    if (g_input_capture) {
        g_isr_load_level = level;
        uint16_t next = (uint16_t)((g_load_head + 1U) % LOAD_ISR_QUEUE_SIZE);
        if (next != g_load_tail) {
            g_load_queue[g_load_head].time_us = edge_us;
            g_load_queue[g_load_head].level = level;
            g_load_head = next;
        } else {
            g_load_overflow = true;
        }
    }
    portEXIT_CRITICAL_ISR(&g_input_mux);
}

/* [新增] LOAD 未裝載／HOLD 時直接卸除 GPIO3 中斷，減少 ISR 呼叫負擔。 */
static void set_rpm_interrupt(bool enabled) {
    if (enabled && !g_measurement_interrupts_attached) {
        attachInterrupt(digitalPinToInterrupt(RPM_IR_GPIO), rpm_ir_isr, RPM_IR_TRIGGER_EDGE);
        g_measurement_interrupts_attached = true;
    } else if (!enabled && g_measurement_interrupts_attached) {
        detachInterrupt(digitalPinToInterrupt(RPM_IR_GPIO));
        g_measurement_interrupts_attached = false;
    }
}

/* [新增] LOAD 已發射後不再接收 LOAD 中斷；HOLD 完成後才恢復裝載偵測。 */
static void set_load_interrupt(bool enabled) {
    if (enabled && !g_load_interrupt_attached) {
        attachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO), load_ir_isr, LOAD_IR_TRIGGER_EDGE);
        g_load_interrupt_attached = true;
    } else if (!enabled && g_load_interrupt_attached) {
        detachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO));
        g_load_interrupt_attached = false;
    }
}

static void disable_input_capture(void) {
    portENTER_CRITICAL(&g_input_mux);
    g_input_capture = false;
    g_rpm_head = 0U;
    g_rpm_tail = 0U;
    g_load_head = 0U;
    g_load_tail = 0U;
    g_rpm_overflow = false;
    g_load_overflow = false;
    portEXIT_CRITICAL(&g_input_mux);
}

static void restart_input_capture(void) {
    /* [修改] 開始、故障重同步及 HOLD 解鎖時，丟棄所有歷史事件。 */
    portENTER_CRITICAL(&g_input_mux);
    g_rpm_head = 0U;
    g_rpm_tail = 0U;
    g_load_head = 0U;
    g_load_tail = 0U;
    g_rpm_overflow = false;
    g_load_overflow = false;
    if (measurement_uses_load()) {
        g_isr_load_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
        g_load_raw = g_isr_load_level;
    } else {
        /* [新增] AUTO 模式使用邏輯 READY，不讀 LOAD GPIO。 */
        g_isr_load_level = LOAD_ACTIVE_LEVEL;
        g_load_raw = LOAD_ACTIVE_LEVEL;
        g_load_stable = LOAD_ACTIVE_LEVEL;
    }
    g_load_candidate_us = micros();
    g_input_capture = true;
    portEXIT_CRITICAL(&g_input_mux);
}

static void reset_measurement(bool loaded) {
    g_raw_period_count = 0U;
    g_max_period_us = 0U;
    g_max_period_index = UINT32_MAX;
    /* [BRD_BBP 新增] 只重置本次曲線，不清除已發布歷史。 */
    brd_bbp_capture_reset();
    /*
        [V0.12 刪減] 此處不清 ISR 佇列，避免清掉較晚但已收到的有效事件。
        一般狀態持續捕捉，WAIT_LOAD 的 RPM 在事件處理時直接忽略。
    */
    g_period_valid = false;
    g_quiet_expired = true;
    g_has_valid_rpm = false;
    g_current_rpm = 0U;
    g_max_rpm = 0U;
    g_auto_launch_candidate = false;
    g_state = (loaded || !measurement_uses_load()) ? BRD_STATE_LOADED_READY : BRD_STATE_WAIT_LOAD;
    g_show_max = false;
    g_display_max = 0U;
    g_max_lock = false;
    /* [V1.2.2 重置] 真正開始新量測後，結束上一輪的解除裝載閘門。 */
    g_load_release_required = false;
    g_max_frame_seen = false;
    g_display_generation++;
    update_modification_window();
}

/*
    [新增] 只有停止取樣後才執行 BBP 資料轉換。
    原始 RAM 全數保留到下一次 reset；BLE 固定 32 點，在完整取樣區間均勻取點。
    若峰值仍在 RAM 內，額外確保其位置被選入 BLE 曲線。
    MAX 使用原始峰值週期獨立寫入，避免協定 32 點限制改變最高轉速。
*/
static void publish_stopped_capture(void) {
    if (g_raw_period_count == 0U || g_max_period_us == 0U) {
        return;
    }

    /* [新增] 此筆只更新 BBP 代表 MAX，不占用 32 點曲線。 */
    brd_bbp_capture_period(g_max_period_us, g_launch_us, false);
    const uint32_t count = g_raw_period_count;
    const uint32_t points = count < RPM_BLE_CURVE_POINTS ? count : RPM_BLE_CURVE_POINTS;
    uint32_t selected[RPM_BLE_CURVE_POINTS] = {};
    uint32_t nearest = 0U;
    uint32_t nearest_distance = UINT32_MAX;
    for (uint32_t index = 0U; index < points; ++index) {
        selected[index] = points == 1U ? 0U :
            static_cast<uint32_t>((static_cast<uint64_t>(index) * (count - 1U)) / (points - 1U));
        if (g_max_period_index < count) {
            const uint32_t distance = selected[index] > g_max_period_index ?
                selected[index] - g_max_period_index : g_max_period_index - selected[index];
            if (distance < nearest_distance) {
                nearest_distance = distance;
                nearest = index;
            }
        }
    }
    if (points != 0U && g_max_period_index < count) {
        /* [新增] 抽點不濾波，僅以最接近峰值的槽位替換成真實 MAX 週期。 */
        selected[nearest] = g_max_period_index;
    }
    for (uint32_t index = 0U; index < points; ++index) {
        brd_bbp_capture_period(g_raw_periods[selected[index]], g_launch_us, true);
    }
    brd_bbp_capture_launch(g_launch_us);
    brd_bbp_capture_finish();
}

static void finish_measurement(void) {
    if (g_state != BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }
    if (!g_has_valid_rpm) {
        reset_measurement(false);
        return;
    }

    /*
        [V1.2.3 修改] 結束條件成立後，先禁止 ISR 收集 RPM/LOAD，再進入獨立 HOLD。
        HOLD 期間一律不歸零、不接受新 LOAD、不啟動下一輪；BLE 的結果快照
        在進入 HOLD 後才凍結，由主迴圈發送 MAX 與曲線，ISR 不發 BLE。
    */
    disable_input_capture();
    set_rpm_interrupt(false);
    set_load_interrupt(false);
    g_display_max = g_max_rpm;
    g_show_max = true;
    g_display_generation++;
    g_max_lock = true;
    g_load_release_required = measurement_uses_load();
    g_max_frame_seen = false;
    g_max_lock_start_ms = millis();
    g_state = BRD_STATE_HOLD;
    g_current_rpm = 0U;
    g_period_valid = false;
    g_quiet_expired = true;
    update_modification_window();
    /* [修改] ISR 已停收，現在才整理曲線並封存，量測期間不呼叫 BLE Session。 */
    publish_stopped_capture();
}

static void check_launch_threshold(uint32_t event_us, bool quiet_timeout = false) {
    /*
        [修改] 每一筆即時 RPM 直接比較設定的停止百分比，首筆達標立即結束。
        LOAD 延續 <=；AUTO 延續 <，且仍須先達 AUTO 最低 RPM 門檻。
        300 ms 無脈衝歸零沿用原保護：沒有新的 Falling Edge 也能結束發射。
    */
    (void)quiet_timeout;
    if (g_state != BRD_STATE_SPINNING_LOADED || !g_has_valid_rpm || g_max_rpm == 0U ||
        (!measurement_uses_load() && !g_auto_launch_candidate)) {
        return;
    }
    const uint32_t current_scaled = static_cast<uint32_t>(g_current_rpm) * 100UL;
    const uint32_t finish_scaled = static_cast<uint32_t>(g_max_rpm) *
                                   brd_settings_get().finish_percent;
    const bool reached_finish = measurement_uses_load() ?
        (current_scaled <= finish_scaled) : (current_scaled < finish_scaled);
    if (!reached_finish) {
        return;
    }

    g_launch_us = event_us;
    g_state = BRD_STATE_SPINNING_LAUNCHED;
    increment_counter(g_diagnostics.launch_events);
    finish_measurement();
}

static void process_rpm_edge(uint32_t edge_us) {
    /* [保留] LOAD 未裝載時不採樣；AUTO 看到第一次 Falling Edge 後開始。 */
    if (g_state == BRD_STATE_WAIT_LOAD) {
        if (measurement_uses_load()) {
            return;
        }
        g_state = BRD_STATE_LOADED_READY;
    }
    if (g_state == BRD_STATE_LOADED_READY) {
        if (!measurement_uses_load() && g_show_max) {
            reset_measurement(true);
        }
        g_state = BRD_STATE_SPINNING_LOADED;
        /* [新增] 開始擷取後 LOAD 腳已無判定功能，立即停止 LOAD ISR。 */
        set_load_interrupt(false);
        update_modification_window();
    }

    if (!g_period_valid) {
        /* [修改] 第一次 FALLING 只建立時間基準；第二次起每圈一筆。 */
        g_last_edge_us = edge_us;
        g_last_activity_us = edge_us;
        g_period_valid = true;
        g_quiet_expired = false;
        return;
    }

    /* [修改] 32-bit 無號相減，自動支援 micros() 約 71 分鐘回繞。 */
    const uint32_t period_us = static_cast<uint32_t>(edge_us - g_last_edge_us);
    g_last_edge_us = edge_us;
    g_last_activity_us = edge_us;
    g_quiet_expired = false;
    if (period_us == 0U) {
        /* [必要邊界] 避免除以零，非零期間一律不濾波。 */
        return;
    }

    /* [新增] 首先原樣保存整圈時間，不進行排序／中位數／抽點。 */
    const bool period_recorded = g_raw_period_count < RPM_RAW_CAPTURE_CAPACITY;
    if (period_recorded) {
        g_raw_periods[g_raw_period_count++] = period_us;
    } else {
        increment_counter(g_diagnostics.raw_capture_overflows);
    }
    increment_counter(g_diagnostics.raw_periods_seen);

    /* [修改] 60 秒/週期/每圈脈衝數；協定僅能顯示 0..60000 RPM。 */
    const uint32_t rpm = (60000000UL / period_us) / PULSES_PER_REV;
    g_current_rpm = static_cast<uint16_t>(rpm > RPM_VALID_MAX ? RPM_VALID_MAX : rpm);
    if (g_current_rpm != 0U) {
        g_has_valid_rpm = true;
    }
    if (g_current_rpm > g_max_rpm) {
        g_max_rpm = g_current_rpm;
        /* [新增] 即便全量 RAM 滿，也保留 MAX 對應的未處理原始週期。 */
        g_max_period_us = rpm > RPM_VALID_MAX ? (60000000UL / RPM_VALID_MAX) : period_us;
        g_max_period_index = period_recorded ? g_raw_period_count - 1U : UINT32_MAX;
    }
    if (!measurement_uses_load() && !g_auto_launch_candidate &&
        g_current_rpm >= brd_settings_get().auto_min_rpm) {
        g_auto_launch_candidate = true;
    }
    check_launch_threshold(edge_us);
}

// [修改] confirmed_us 是 LOAD 電位維持 cfg 去抖時間（預設 50 ms）後的確認時間。
static void handle_load_change(uint32_t confirmed_us) {
    (void)confirmed_us;
    if (!measurement_uses_load()) {
        return;
    }

    g_load_stable = g_load_raw;
    increment_counter(g_diagnostics.load_stable_transitions);
    /*
        [V1.2.2 修正] HOLD 後尚未確認真正解除裝載時，
        即使 GPIO1 已經維持 HIGH 50ms，也不准重置 MAX 或重新開始 RPM。
        LOW 的獨立解除裝載計時由 INPUT_TIMER_LOAD_RELEASE 處理。
    */
    if (g_load_release_required) {
        return;
    }

    /*
        [修正] RPM 已開始後，LOAD 只更新穩定顯示狀態，不得再重置量測。
        LOAD HIGH / LOW 都不參與發射成功，也不能清除 RPM / MAX / 曲線。
    */
    if (g_state == BRD_STATE_SPINNING_LOADED ||
        g_state == BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }

    if (g_load_stable == LOAD_ACTIVE_LEVEL) {
        /*
            [修正] 只有 WAIT_LOAD -> LOAD 穩定 HIGH cfg 去抖時間 才能啟動新一輪量測。
            已經 READY 或 SPINNING 時再次 HIGH 不得重置正在進行的資料。
        */
        if (g_state == BRD_STATE_WAIT_LOAD) {
            reset_measurement(true);
            /* [修改] 只有裝載去抖成立才啟用 GPIO3 FALLING ISR。 */
            set_rpm_interrupt(true);
        }
        return;
    }

    if (g_state == BRD_STATE_LOADED_READY) {
        /* [保留] 尚未出現 RPM 就取消裝載時，回 WAIT_LOAD 並維持歸零。 */
        reset_measurement(false);
        set_rpm_interrupt(false);
    }
}

enum input_timer_t {
    INPUT_TIMER_NONE,
    INPUT_TIMER_LOAD,
    /* [V1.2.2 新增] 即使舊 g_load_stable 恰好是 LOW，也驗證實體 LOW 持續去抖時間。 */
    INPUT_TIMER_LOAD_RELEASE,
    INPUT_TIMER_ZERO,
    INPUT_TIMER_IDLE,
    INPUT_TIMER_EMPTY_LAUNCH,
    INPUT_TIMER_AUTO_RESET
};

static void consider_timer(uint32_t deadline, input_timer_t type, uint32_t until_us,
                           bool include_equal, input_timer_t &selected, uint32_t &selected_us) {
    bool due = time_before(deadline, until_us) || (include_equal && deadline == until_us);
    if (due && (selected == INPUT_TIMER_NONE || time_before(deadline, selected_us))) {
        selected = type;
        selected_us = deadline;
    }
}

static void advance_input_time(uint32_t until_us, bool include_equal_timeouts,
                               bool suppress_rpm_timeouts) {
    /*
        [保留] 事件時間排序、micros 回繞與溢位期間的 timeout 暫停。
        [修改] 已達有效門檻的 AUTO 與 LOAD 共用無脈衝歸零備援。
        [保留] AUTO 未達門檻時，250ms 無有效 RPM 重置，不發布 MAX。
    */
    for (uint8_t pass = 0U; pass < 6U && !g_max_lock; pass++) {
        input_timer_t timer = INPUT_TIMER_NONE;
        uint32_t deadline = 0UL;
        if (measurement_uses_load()) {
            if (g_load_raw != g_load_stable) {
                consider_timer(static_cast<uint32_t>(g_load_candidate_us + LOAD_IR_DEBOUNCE_US),
                               INPUT_TIMER_LOAD, until_us, true, timer, deadline);
            }
            /*
                [V1.2.2 新增] HOLD 到期後才重新開始觀察 GPIO1。
                LOW 必須真實維持完整 LOAD_IR_DEBOUNCE_US，不能靠程式指定 LOW。
                即使原穩定狀態原本就是 LOW，也能完成這次實體釋放確認。
            */
            if (g_load_release_required && g_load_raw != LOAD_ACTIVE_LEVEL) {
                consider_timer(static_cast<uint32_t>(g_load_candidate_us + LOAD_IR_DEBOUNCE_US),
                               INPUT_TIMER_LOAD_RELEASE, until_us, true, timer, deadline);
            }
            if (!suppress_rpm_timeouts) {
                bool spinning = g_state == BRD_STATE_SPINNING_LOADED ||
                                g_state == BRD_STATE_SPINNING_LAUNCHED;
                if (spinning && !g_quiet_expired) {
                    consider_timer(static_cast<uint32_t>(g_last_activity_us + RPM_ZERO_TIMEOUT_MS * 1000UL),
                                   INPUT_TIMER_ZERO, until_us, include_equal_timeouts, timer, deadline);
                }
                if (g_state == BRD_STATE_SPINNING_LOADED && !g_has_valid_rpm) {
                    consider_timer(static_cast<uint32_t>(g_last_activity_us + PRELAUNCH_IDLE_RESET_MS * 1000UL),
                                   INPUT_TIMER_IDLE, until_us, include_equal_timeouts, timer, deadline);
                }
                if (g_state == BRD_STATE_SPINNING_LAUNCHED && !g_has_valid_rpm) {
                    consider_timer(static_cast<uint32_t>(g_launch_us + POST_LAUNCH_NO_RPM_TIMEOUT_MS * 1000UL),
                                   INPUT_TIMER_EMPTY_LAUNCH, until_us, include_equal_timeouts, timer, deadline);
                }
            }
        } else if (!suppress_rpm_timeouts && g_state == BRD_STATE_SPINNING_LOADED) {
            if (!g_quiet_expired && g_has_valid_rpm) {
                uint32_t zero_ms = g_auto_launch_candidate ? RPM_ZERO_TIMEOUT_MS : AUTO_RESET_ZERO_MS;
                input_timer_t zero_type = g_auto_launch_candidate ? INPUT_TIMER_ZERO : INPUT_TIMER_AUTO_RESET;
                consider_timer(static_cast<uint32_t>(g_last_activity_us + zero_ms * 1000UL),
                               zero_type, until_us, include_equal_timeouts, timer, deadline);
            }
            if (!g_has_valid_rpm) {
                consider_timer(static_cast<uint32_t>(g_last_activity_us + PRELAUNCH_IDLE_RESET_MS * 1000UL),
                               INPUT_TIMER_IDLE, until_us, include_equal_timeouts, timer, deadline);
            }
        }
        if (timer == INPUT_TIMER_NONE) { return; }
        if (timer == INPUT_TIMER_LOAD) {
            handle_load_change(deadline);
        } else if (timer == INPUT_TIMER_LOAD_RELEASE) {
            /* [V1.2.2 修正] 解除裝載已通過真實 LOW 去抖；等待下一次 HIGH。 */
            g_load_stable = g_load_raw;
            g_load_release_required = false;
        } else if (timer == INPUT_TIMER_ZERO) {
            g_current_rpm = 0U;
            g_period_valid = false;
            g_quiet_expired = true;
            check_launch_threshold(deadline, true);
        } else if (timer == INPUT_TIMER_IDLE) {
            reset_measurement(!measurement_uses_load() || g_load_stable == LOAD_ACTIVE_LEVEL);
        } else if (timer == INPUT_TIMER_EMPTY_LAUNCH) {
            finish_measurement();
        } else if (timer == INPUT_TIMER_AUTO_RESET) {
            reset_measurement(true);
        }
    }
}

static void snapshot_inputs(uint16_t &rpm_count, uint16_t &load_count, uint32_t &now_us,
                            bool &rpm_overflow, bool &load_overflow) {
    rpm_count = 0U;
    load_count = 0U;
    portENTER_CRITICAL(&g_input_mux);
    rpm_overflow = g_rpm_overflow;
    load_overflow = measurement_uses_load() ? g_load_overflow : false;
    g_rpm_overflow = false;
    g_load_overflow = false;
    while (g_rpm_tail != g_rpm_head && rpm_count < RPM_ISR_QUEUE_SIZE) {
        g_rpm_batch[rpm_count] = g_rpm_queue[g_rpm_tail];
        rpm_count++;
        g_rpm_tail = (uint16_t)((g_rpm_tail + 1U) % RPM_ISR_QUEUE_SIZE);
    }
    if (measurement_uses_load()) {
        while (g_load_tail != g_load_head && load_count < LOAD_ISR_QUEUE_SIZE - 1U) {
            g_load_batch[load_count].time_us = g_load_queue[g_load_tail].time_us;
            g_load_batch[load_count].level = g_load_queue[g_load_tail].level;
            load_count++;
            g_load_tail = (uint16_t)((g_load_tail + 1U) % LOAD_ISR_QUEUE_SIZE);
        }
    } else {
        /* [新增] AUTO 不累積、不處理 LOAD 事件。 */
        g_load_tail = g_load_head;
    }
    now_us = micros();
    if (measurement_uses_load()) {
        int actual_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
        if (actual_level != g_isr_load_level && !load_overflow) {
            /* [V0.12 新增] 補捉尚未送達的 ISR；補捉時間起重新完整去抖。 */
            g_load_batch[load_count].time_us = now_us;
            g_load_batch[load_count].level = actual_level;
            load_count++;
            g_isr_load_level = actual_level;
        }
    }
    portEXIT_CRITICAL(&g_input_mux);
}

void brd_measurement_begin(void) {
    /* [V0.12 新增] 允許主 loop 重複呼叫；ADC 故障恢復時才重新建立量測。 */
    if (g_measurement_enabled) {
        return;
    }
    g_measurement_stopped = false;
    reset_measurement(!measurement_uses_load());
    g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    restart_input_capture();
    g_measurement_enabled = true;
    set_rpm_interrupt(!measurement_uses_load());
    set_load_interrupt(measurement_uses_load());
    update_modification_window();
}

void brd_measurement_stop(void) {
    if (g_measurement_stopped) {
        return;
    }
    g_measurement_stopped = true;
    g_measurement_enabled = false;
    disable_input_capture();
    set_rpm_interrupt(false);
    set_load_interrupt(false);
    reset_measurement(!measurement_uses_load());
    g_load_raw = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    /* [BRD_BBP 新增] 低電/ADC 故障中斷的一發不發布。 */
    brd_bbp_capture_abort();
}

void brd_measurement_update(void) {
    if (!g_measurement_enabled) {
        return;
    }
    if (g_max_lock) {
        /* [V1.4.4 修改] LOAD/AUTO 共用 SAVE 成功後生效的 HOLD 時間。 */
        const uint32_t hold_interval_ms = brd_settings_get().hold_ms;
        if ((uint32_t)(millis() - g_max_lock_start_ms) < hold_interval_ms) {
            return;
        }
        /*
            [V1.4.2 修改] HOLD 結束：LOAD 在 2.5 秒後正式回 WAIT LOAD。
            不清除 MAX、不偽造 LOAD LOW，也不因原腳位維持 HIGH 而進 LOADED。
            下一輪必須觀察 LOW 穩定 50ms，再收到 HIGH 穩定 50ms。
        */
        g_max_lock = false;
        g_state = BRD_STATE_WAIT_LOAD;
        if (measurement_uses_load()) {
            g_load_release_required = true;
        } else {
            /* [新增] AUTO HOLD 結束後回邏輯 READY；MAX 保留到下一次 RPM 啟動。 */
            g_state = BRD_STATE_LOADED_READY;
            g_current_rpm = 0U;
            g_has_valid_rpm = false;
            g_period_valid = false;
            g_quiet_expired = true;
            g_auto_launch_candidate = false;
        }
        update_modification_window();
        restart_input_capture();
        set_rpm_interrupt(!measurement_uses_load());
        set_load_interrupt(measurement_uses_load());
        return;
    }

    uint16_t rpm_count;
    uint16_t load_count;
    uint32_t now_us;
    bool rpm_overflow;
    bool load_overflow;
    snapshot_inputs(rpm_count, load_count, now_us, rpm_overflow, load_overflow);
    if (rpm_overflow) {
        /*
            [修正] RPM Queue overflow 不再 abort 整次 Capture。
            snapshot 內已保存的事件仍依序處理，保留已取得的原始週期與 MAX。
            缺失區段在批次處理完成後只重新建立 Falling 週期基準，避免把 Queue overflow
            誤判成 300 ms 無脈衝而提前完成發射。
        */
        increment_counter(g_diagnostics.rpm_queue_overflows);
    }
    if (measurement_uses_load() && load_overflow) {
        /*
            [修正] LOAD Queue overflow 不得中止 RPM 採樣。
            LOAD 在 SPINNING 階段不參與發射判定；即使 LOAD 邊沿遺失，也只丟棄
            本批 LOAD 歷史並從目前實體電位重新做 cfg 去抖時間。
        */
        increment_counter(g_diagnostics.load_queue_overflows);
        load_count = 0U;
        portENTER_CRITICAL(&g_input_mux);
        g_isr_load_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
        g_load_raw = g_isr_load_level;
        g_load_candidate_us = now_us;
        g_load_head = 0U;
        g_load_tail = 0U;
        g_load_overflow = false;
        portEXIT_CRITICAL(&g_input_mux);
    }

    uint16_t rpm_index = 0U;
    uint16_t load_index = 0U;
    while ((rpm_index < rpm_count || load_index < load_count) && !g_max_lock) {
        bool use_rpm = rpm_index < rpm_count &&
            (load_index >= load_count ||
             !time_before(g_load_batch[load_index].time_us, g_rpm_batch[rpm_index]));
        uint32_t event_us = use_rpm ? g_rpm_batch[rpm_index] : g_load_batch[load_index].time_us;
        advance_input_time(event_us, false, rpm_overflow);
        if (g_max_lock) {
            break;
        }
        if (use_rpm) {
            process_rpm_edge(g_rpm_batch[rpm_index++]);
        } else {
            const input_edge_t &edge = g_load_batch[load_index++];
            if (measurement_uses_load()) {
                g_load_raw = edge.level;
                g_load_candidate_us = edge.time_us;
            }
        }
    }
    if (!g_max_lock && rpm_overflow) {
        /*
            [修改] Queue overflow 代表部分 Falling Edge 可能遺失；中斷前後跨缺口
            不可當作一圈計算，只清除週期基準重新同步。
            同時把 quiet timeout 起點移到本次重新同步時間，避免立即觸發 300 ms 歸零。
        */
        g_period_valid = false;
        if (g_state == BRD_STATE_SPINNING_LOADED && g_has_valid_rpm) {
            g_last_activity_us = now_us;
            g_quiet_expired = false;
        }
    }
    if (!g_max_lock) {
        advance_input_time(now_us, true, false);
    }
    update_modification_window();
}

brd_display_t brd_measurement_get_display(void) {
    brd_display_t display;
    /* [修改] AUTO 的 BLE 裝載旗標直接跟隨即時 RPM；OLED 文字另由 AUTO READY 固定顯示。 */
    /*
        [V1.2.3 修改] LOAD 顯示以有效量測狀態為準，不能讓 HOLD 結束時
        GPIO1 尚 HIGH 就誤顯示 LOADED READY。進行中的量測仍沿用原版
        去抖 LOAD 狀態作為 BLE bit2；下一輪必須真實 LOW->HIGH 去抖。
    */
    display.loaded = measurement_uses_load() ?
                     (g_state != BRD_STATE_WAIT_LOAD &&
                      g_state != BRD_STATE_HOLD &&
                      g_load_stable == LOAD_ACTIVE_LEVEL) :
                     (g_current_rpm != 0U);
    display.show_max = g_show_max;
    display.value = g_show_max ? g_display_max : g_current_rpm;
    display.generation = g_display_generation;
    display.hold_active = g_show_max && g_max_lock;
    return display;
}

brd_measurement_diagnostics_t brd_measurement_get_diagnostics(void) {
    return g_diagnostics;
}

void brd_measurement_max_frame_presented(uint32_t generation) {
    if (!g_measurement_enabled || !g_show_max ||
        generation != g_display_generation || g_max_frame_seen) {
        return;
    }
    /* [V1.2.3 修正] 只有目前仍在 HOLD 才接受 OLED 回報；過期幀不能再鎖住量測。 */
    if (g_state != BRD_STATE_HOLD || !g_max_lock) {
        return;
    }
    g_max_frame_seen = true;
    g_max_lock_start_ms = millis();
    disable_input_capture();
}

/* [修改] LOAD 維持 WAIT_LOAD；AUTO 的 READY 亦屬非量測安全時段。 */
bool brd_measurement_storage_allowed(void) {
    return !g_measurement_enabled || g_state == BRD_STATE_WAIT_LOAD ||
           (!measurement_uses_load() && g_state == BRD_STATE_LOADED_READY);
}

/* [新增] callback 與主 loop 共用安全窗口；已排入 GPIO 事件時先處理量測。 */
bool brd_measurement_modifications_allowed(void) {
    portENTER_CRITICAL(&g_input_mux);
    bool allowed = g_modification_window && !g_rpm_overflow && !g_load_overflow &&
                   g_rpm_head == g_rpm_tail && g_load_head == g_load_tail && !g_settings_paused;
    portEXIT_CRITICAL(&g_input_mux);
    return allowed;
}
bool brd_measurement_pause_for_settings(void) {
    portENTER_CRITICAL(&g_input_mux);
    bool allowed = g_modification_window && !g_rpm_overflow && !g_load_overflow &&
                   g_rpm_head == g_rpm_tail && g_load_head == g_load_tail && !g_settings_paused;
    if (allowed) {
        g_settings_paused = true;
        g_input_capture = false;
    }
    portEXIT_CRITICAL(&g_input_mux);
    return allowed;
}
void brd_measurement_resume_after_settings(bool applied) {
    if (!g_settings_paused) { return; }
    if (applied && g_measurement_enabled) {
        /* [修改] 設定切換後重新同步 RPM/LOAD 中斷啟用條件。 */
        set_load_interrupt(measurement_uses_load());
        g_period_valid = false;
        g_quiet_expired = true;
        g_has_valid_rpm = false;
        g_current_rpm = g_max_rpm = 0U;
        g_auto_launch_candidate = false;
        g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
        g_state = !measurement_uses_load() && !g_max_lock ?
                  BRD_STATE_LOADED_READY : BRD_STATE_WAIT_LOAD;
    }
    portENTER_CRITICAL(&g_input_mux);
    g_settings_paused = false;
    portEXIT_CRITICAL(&g_input_mux);
    update_modification_window();
    if (g_measurement_enabled && !g_max_lock) {
        restart_input_capture();
        set_rpm_interrupt(!measurement_uses_load() || g_state == BRD_STATE_LOADED_READY);
    }
}

/* [新增] 主 loop 以此禁止量測期間執行 Notify/歷史寫入/OTA 排程。 */
bool brd_measurement_capture_active(void) {
    return g_measurement_enabled &&
           (g_state == BRD_STATE_SPINNING_LOADED || g_state == BRD_STATE_SPINNING_LAUNCHED);
}
