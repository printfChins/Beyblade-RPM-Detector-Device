/*
    檔案位置: BRD_OLED/brd_measurement.cpp
    貼放位置: 以本檔完整替換，同資料夾保留 brd_measurement.h。
    [修改] LOAD/AUTO、AUTO 門檻及結束比例直接使用 brd_config.h。
    [刪減] 無線量測紀錄、協議封存呼叫、Flash 寫入窗口與設定暫停介面。
    [保留] ISR 佇列、第一邊沿極性、RPM 換算、去抖、溢位處理及 MAX 自鎖。
*/
#include <soc/gpio_struct.h>

#include "brd_config.h"
#include "brd_measurement.h"

struct input_edge_t {
    uint32_t time_us;
    int level;
};

static portMUX_TYPE g_input_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile input_edge_t g_rpm_queue[RPM_ISR_QUEUE_SIZE];
static volatile input_edge_t g_load_queue[LOAD_ISR_QUEUE_SIZE];
static volatile uint16_t g_rpm_head = 0U;
static volatile uint16_t g_rpm_tail = 0U;
static volatile uint16_t g_load_head = 0U;
static volatile uint16_t g_load_tail = 0U;
static volatile bool g_rpm_overflow = false;
static volatile bool g_load_overflow = false;
static volatile bool g_input_capture = false;
/* [修改] -1=尚未選定；HIGH=正緣參考；LOW=負緣參考。ISR 選定後只收參考極性，降低 Queue 負載。 */
static volatile int g_rpm_reference_level = -1;
static volatile int g_isr_load_level = LOW;

/* [V0.12 新增] 固定 RAM 快照，不使用 heap，也不把陣列放在主迴圈堆疊。 */
static input_edge_t g_rpm_batch[RPM_ISR_QUEUE_SIZE];
static input_edge_t g_load_batch[LOAD_ISR_QUEUE_SIZE];
static brd_measurement_diagnostics_t g_diagnostics = {};
static bool g_measurement_enabled = false;

static bool g_measurement_stopped = false;
static bool g_measurement_interrupts_attached = false;
static bool g_load_interrupt_attached = false;
static brd_state_t g_state = BRD_STATE_WAIT_LOAD;
static int g_load_raw = LOW;
static int g_load_stable = LOW;
static uint32_t g_load_candidate_us = 0UL;
/* [修改] HIGH/LOW 分別代表正緣/負緣週期基準；每次量測只會使用先觸發的參考極性。 */
static bool g_period_valid[2] = {false, false};
static bool g_quiet_expired = true;
static uint32_t g_last_edge_us[2] = {0UL, 0UL};
static uint32_t g_last_activity_us = 0UL;
static uint32_t g_launch_us = 0UL;
static bool g_has_valid_rpm = false;
static uint16_t g_current_rpm = 0U;
static uint16_t g_max_rpm = 0U;
/* [新增] AUTO 達到執行時最低 RPM 後保存資格，共用百分比結束。 */
static bool g_auto_launch_candidate = false;
/* [刪減] AUTO 不再保存一秒零轉速候選時間戳記。 */
static bool g_show_max = false;
static uint16_t g_display_max = 0U;
static uint32_t g_display_generation = 0UL;
static bool g_max_lock = false;
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
    /* [修改] 單機版模式固定由 brd_config.h 選擇。 */
    return BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD;
}

static void IRAM_ATTR rpm_ir_isr(void) {
    uint32_t edge_us = micros();
    int level = (int)((GPIO.in.val >> RPM_IR_GPIO) & 1U);
    portENTER_CRITICAL_ISR(&g_input_mux);
    if (g_input_capture && (g_rpm_reference_level < 0 || level == g_rpm_reference_level)) {
        /*
            [修改] 尚未選定參考時先收正、負緣，讓主迴圈依事件時間決定真正的第一個邊沿。
            一旦參考極性確定，ISR 只收同極性事件，避免 CHANGE 讓 Queue 負載加倍。
        */
        uint16_t next = (uint16_t)((g_rpm_head + 1U) % RPM_ISR_QUEUE_SIZE);
        if (next != g_rpm_tail) {
            g_rpm_queue[g_rpm_head].time_us = edge_us;
            g_rpm_queue[g_rpm_head].level = level;
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

    /*
        [V0.12 刪減] 此處不清 ISR 佇列，避免清掉較晚但已收到的有效事件。
        一般狀態持續捕捉，WAIT_LOAD 的 RPM 在事件處理時直接忽略。
    */
    g_period_valid[LOW] = g_period_valid[HIGH] = false;
    portENTER_CRITICAL(&g_input_mux);
    g_rpm_reference_level = -1;
    portEXIT_CRITICAL(&g_input_mux);
    g_quiet_expired = true;
    g_has_valid_rpm = false;
    g_current_rpm = 0U;
    g_max_rpm = 0U;
    g_auto_launch_candidate = false;
    g_state = (loaded || !measurement_uses_load()) ? BRD_STATE_LOADED_READY : BRD_STATE_WAIT_LOAD;
    g_show_max = false;
    g_display_max = 0U;
    g_max_lock = false;
    g_max_frame_seen = false;
    g_display_generation++;
}

static void finish_measurement(void) {
    if (g_state != BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }
    if (!g_has_valid_rpm) {
        reset_measurement(false);
        return;
    }

    g_display_max = g_max_rpm;
    g_show_max = true;
    g_display_generation++;
    g_max_lock = true;
    g_max_frame_seen = false;
    g_max_lock_start_ms = millis();
    g_state = BRD_STATE_WAIT_LOAD;
    g_current_rpm = 0U;
    g_period_valid[LOW] = g_period_valid[HIGH] = false;
    g_quiet_expired = true;
    disable_input_capture();
}

static void check_launch_threshold(uint32_t event_us) {
    /*
        [修改] LOAD / AUTO 共用嚴格百分比結束條件。
        [新增] AUTO 必須先達最低 RPM 門檻，才可發布有效結果。
        [刪減] AUTO 等待 RPM=0 滿一秒的獨立發射邏輯。
        使用交叉乘法保留百分比小數邊界，相等時不結束。
    */
    if (g_state != BRD_STATE_SPINNING_LOADED || !g_has_valid_rpm || g_max_rpm == 0U ||
        (!measurement_uses_load() && !g_auto_launch_candidate)) {
        return;
    }
    if (static_cast<uint32_t>(g_current_rpm) * 100UL <
        static_cast<uint32_t>(g_max_rpm) * POST_LAUNCH_FINISH_PERCENT) {
        g_launch_us = event_us;
        g_state = BRD_STATE_SPINNING_LAUNCHED;

        increment_counter(g_diagnostics.launch_events);
        finish_measurement();
    }
}

static void process_rpm_edge(const input_edge_t &edge) {
    /* [修改] LOAD 模式在未完成裝載啟動前完全忽略 RPM。 */
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
    }

    const uint32_t edge_us = edge.time_us;
    const uint8_t polarity = static_cast<uint8_t>(edge.level);

    if (g_rpm_reference_level < 0) {
        /*
            [修改] 本次檢測的第一個實際 RPM 邊沿決定固定參考極性。
            HIGH 表示先觸發正緣，後續只使用正緣 -> 正緣；
            LOW 表示先觸發負緣，後續只使用負緣 -> 負緣。
        */
        portENTER_CRITICAL(&g_input_mux);
        if (g_rpm_reference_level < 0) {
            g_rpm_reference_level = edge.level;
        }
        portEXIT_CRITICAL(&g_input_mux);
    }

    if (edge.level != g_rpm_reference_level) {
        /* [修改] 參考極性選定後，先前已排入 Queue 的另一極性事件直接忽略。 */
        return;
    }

    if (!g_period_valid[polarity]) {
        /* [修改] 第一個參考邊沿只建立基準，下一個同極性邊沿才產生第一筆 RPM。 */
        g_last_edge_us[polarity] = edge_us;
        g_last_activity_us = edge_us;
        g_period_valid[polarity] = true;
        g_quiet_expired = false;
        return;
    }

    const uint32_t period_us = static_cast<uint32_t>(edge_us - g_last_edge_us[polarity]);
    if (period_us < RPM_MIN_PERIOD_US) {
        return;
    }
    if (period_us > RPM_MAX_PERIOD_US) {
        g_last_edge_us[polarity] = edge_us;
        g_last_activity_us = edge_us;
        g_current_rpm = 0U;
        g_quiet_expired = false;
        check_launch_threshold(edge_us);
        return;
    }

    const uint32_t rpm = 60000000UL / period_us / PULSES_PER_REV;
    if (rpm == 0U || rpm > RPM_VALID_MAX) {
        return;
    }

    g_last_edge_us[polarity] = edge_us;
    g_last_activity_us = edge_us;
    g_quiet_expired = false;
    g_current_rpm = static_cast<uint16_t>(rpm);
    g_has_valid_rpm = true;

    if (g_current_rpm > g_max_rpm) {
        g_max_rpm = g_current_rpm;
    }

    if (!measurement_uses_load() && !g_auto_launch_candidate &&
        g_current_rpm >= AUTO_RPM_THRESHOLD) {
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
        }
        return;
    }

    if (g_state == BRD_STATE_LOADED_READY) {
        /* [保留] 尚未出現 RPM 就取消裝載時，回 WAIT_LOAD 並維持歸零。 */
        reset_measurement(false);
    }
}

enum input_timer_t {
    INPUT_TIMER_NONE,
    INPUT_TIMER_LOAD,
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
        } else if (timer == INPUT_TIMER_ZERO) {
            g_current_rpm = 0U;
            g_period_valid[LOW] = g_period_valid[HIGH] = false;
            g_quiet_expired = true;
            check_launch_threshold(deadline);
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
        g_rpm_batch[rpm_count].time_us = g_rpm_queue[g_rpm_tail].time_us;
        g_rpm_batch[rpm_count].level = g_rpm_queue[g_rpm_tail].level;
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
    attachInterrupt(digitalPinToInterrupt(RPM_IR_GPIO), rpm_ir_isr, RPM_IR_TRIGGER_EDGE);
    g_measurement_interrupts_attached = true;
    if (measurement_uses_load()) {
        attachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO), load_ir_isr, LOAD_IR_TRIGGER_EDGE);
        g_load_interrupt_attached = true;
    } else {
        g_load_interrupt_attached = false;
    }
    g_measurement_enabled = true;
}

void brd_measurement_stop(void) {
    if (g_measurement_stopped) {
        return;
    }
    g_measurement_stopped = true;
    g_measurement_enabled = false;
    disable_input_capture();
    if (g_measurement_interrupts_attached) {
        detachInterrupt(digitalPinToInterrupt(RPM_IR_GPIO));
        g_measurement_interrupts_attached = false;
    }
    if (g_load_interrupt_attached) {
        detachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO));
        g_load_interrupt_attached = false;
    }
    reset_measurement(!measurement_uses_load());
    g_load_raw = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;

}

void brd_measurement_update(void) {
    if (!g_measurement_enabled) {
        return;
    }
    if (g_max_lock) {
        if ((uint32_t)(millis() - g_max_lock_start_ms) < OLED_MAX_HOLD_MS) {
            return;
        }
        g_max_lock = false;
        /* [修改] HOLD 結束代表下一次量測週期，重新等待第一個正/負緣決定參考。 */
        g_rpm_reference_level = -1;
        if (measurement_uses_load()) {
            g_load_stable = LOW;
        } else {
            /* [新增] AUTO HOLD 結束後回邏輯 READY；MAX 保留到下一次 RPM 啟動。 */
            g_state = BRD_STATE_LOADED_READY;
            g_current_rpm = 0U;
            g_has_valid_rpm = false;
            g_period_valid[LOW] = g_period_valid[HIGH] = false;
            g_quiet_expired = true;
            g_auto_launch_candidate = false;
        }
        restart_input_capture();
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
            snapshot 內已保存的事件仍依序處理，保留最早 32 點、已取得的 MAX 與本次參考極性。
            缺失區段在批次處理完成後只重新建立同極性週期基準，避免把 Queue overflow
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
             !time_before(g_load_batch[load_index].time_us, g_rpm_batch[rpm_index].time_us));
        uint32_t event_us = use_rpm ? g_rpm_batch[rpm_index].time_us : g_load_batch[load_index].time_us;
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
            [修正] Queue overflow 代表中間可能遺失參考邊沿，不能拿 overflow 前最後一個
            參考邊沿與 overflow 後第一個同極性邊沿直接計算 RPM。
            保留本次先觸發所選定的參考極性，只清除週期基準重新同步。
            同時把 quiet timeout 起點移到本次重新同步時間，避免立即觸發 300 ms 歸零。
        */
        g_period_valid[LOW] = g_period_valid[HIGH] = false;
        if (g_state == BRD_STATE_SPINNING_LOADED && g_has_valid_rpm) {
            g_last_activity_us = now_us;
            g_quiet_expired = false;
        }
    }
    if (!g_max_lock) {
        advance_input_time(now_us, true, false);
    }
}

brd_display_t brd_measurement_get_display(void) {
    brd_display_t display;

    display.loaded = measurement_uses_load() ?
                     (g_load_stable == LOAD_ACTIVE_LEVEL) : (g_current_rpm != 0U);
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
    g_max_frame_seen = true;
    g_max_lock = true;
    g_max_lock_start_ms = millis();
    disable_input_capture();
}

