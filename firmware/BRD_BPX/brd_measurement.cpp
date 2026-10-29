/*
    檔案位置: BRD_BPX/brd_measurement.cpp
    [V0.12 修改] LOAD / RPM 事件依時間戳記合併處理，去抖使用事件時間。
    [V0.12 新增] 有界 LOAD 佇列、固定大小快照、溢位診斷與重新同步。
    [V0.12 刪減] LOAD 僅保存最後一次電位及 sequence 的處理方式。
    [V0.12 修改] 發射後門檻使用 cfg 的 MAX 設定百分比。
    [保留] 不加入相鄰 RPM 週期合理性檢查或候選峰值濾波。
    [保留] HOLD 2.5 秒、OLED 完整畫面回報、無脈衝 300 ms 結算。
    [修改] RPM 使用 CHANGE；每次量測以第一個實際觸發邊沿固定參考極性。
    [修改] 先正緣則正緣到正緣計算 RPM；先負緣則負緣到負緣計算 RPM，中途不切換。
    [修改] LOAD 只負責裝載後歸零/啟動量測；卸載不參與發射成功判定。
    [修改] 第一筆非 0 RPM 起即保存曲線，BLE Profile 只保留最早 32 點。
    [修改] MAX 與 32 點曲線獨立；32 點之後仍持續更新 MAX。
    [修改] 發射成功只由即時 RPM < 本次 MAX 設定百分比 判定。
    [修正] SPINNING 後 LOAD 變化與 LOAD Queue overflow 不得重置 RPM / MAX / 曲線。
    [修正] RPM Queue overflow 保留已取樣資料與本次參考極性，只重新建立同極性週期基準，不誤觸發 300 ms 結算。
    [新增] 可由 cfg 切換 LOAD / AUTO；AUTO 不以 LOAD 決定開始、發射或歸零。
    ISR 只記錄事件；所有狀態機與 OLED 仍在主 loop 執行。
*/
#include <soc/gpio_struct.h>

#include "brd_config.h"
#include "brd_bbp.h"
#include "brd_measurement.h"
/* [新增] 四項參數讀取執行時設定，ISR 不存取設定模型。 */
#include "brd_settings.h"
#include "brd_capture.h"
/* [修改] RPM 為最高優先；HOLD 只限制顯示，NVS 交易不停止擷取。 */

struct input_edge_t {
    uint32_t time_us;
    int level;
    uint32_t generation;
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
static volatile uint32_t g_rpm_generation = 0U;
static uint32_t g_processed_rpm_generation = 0U;
static bool g_capture_tainted = false;
/* [修改] -1=尚未選定；HIGH=正緣參考；LOW=負緣參考。ISR 選定後只收參考極性，降低 Queue 負載。 */
static volatile int g_rpm_reference_level = -1;
static volatile int g_isr_load_level = LOW;

/* [V0.12 新增] 固定 RAM 快照，不使用 heap，也不把陣列放在主迴圈堆疊。 */
static input_edge_t g_rpm_batch[RPM_ISR_QUEUE_SIZE];
static input_edge_t g_load_batch[LOAD_ISR_QUEUE_SIZE];
static brd_measurement_diagnostics_t g_diagnostics = {};
static bool g_measurement_enabled = false;
/* [新增] BLE callback 只讀 critical-section 保護的修改窗口，不讀量測狀態機。 */
static bool g_modification_window = true;
static bool g_settings_paused = false;
static bool g_measurement_stopped = false;
static bool g_maintenance_paused = false;
static bool g_capture_fault = false;
/* [新增] 尾段脈衝不重複計次，新 LOAD 或安靜 300ms 可重新武裝。 */
static bool g_rearm_wait = false;
static bool g_rearm_seen_low = false;
static uint32_t g_rearm_activity_us = 0U;
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
    uint32_t edge_us = brd_capture_time_us();
    int level = (int)((GPIO.in.val >> RPM_IR_GPIO) & 1U);
    portENTER_CRITICAL_ISR(&g_input_mux);
    /* [新增] 在篩選前記錄是否真的進入 ISR。 */
    if (g_diagnostics.rpm_isr_entries != UINT32_MAX) { g_diagnostics.rpm_isr_entries++; }
    g_diagnostics.last_rpm_isr_us = edge_us;
    if (!g_input_capture) {
        if (g_diagnostics.rpm_capture_disabled != UINT32_MAX) { g_diagnostics.rpm_capture_disabled++; }
    } else if (g_rpm_reference_level >= 0 && level != g_rpm_reference_level) {
        if (g_diagnostics.rpm_polarity_filtered != UINT32_MAX) { g_diagnostics.rpm_polarity_filtered++; }
    } else {
        /*
            [修改] 尚未選定參考時先收正、負緣，讓主迴圈依事件時間決定真正的第一個邊沿。
            一旦參考極性確定，ISR 只收同極性事件，避免 CHANGE 讓 Queue 負載加倍。
        */
        uint16_t next = (uint16_t)((g_rpm_head + 1U) % RPM_ISR_QUEUE_SIZE);
        if (next != g_rpm_tail) {
            g_rpm_queue[g_rpm_head].time_us = edge_us;
            g_rpm_queue[g_rpm_head].level = level;
            g_rpm_queue[g_rpm_head].generation = g_rpm_generation;
            g_rpm_head = next;
            if (g_diagnostics.rpm_events_enqueued != UINT32_MAX) { g_diagnostics.rpm_events_enqueued++; }
            uint32_t depth = (g_rpm_head + RPM_ISR_QUEUE_SIZE - g_rpm_tail) % RPM_ISR_QUEUE_SIZE;
            if (depth > g_diagnostics.max_rpm_queue_depth) { g_diagnostics.max_rpm_queue_depth = depth; }
        } else {
            if (!g_rpm_overflow) { g_rpm_generation++; }
            g_rpm_overflow = true;
            if (g_diagnostics.rpm_events_dropped != UINT32_MAX) { g_diagnostics.rpm_events_dropped++; }
        }
    }
    portEXIT_CRITICAL_ISR(&g_input_mux);
}

static void IRAM_ATTR load_ir_isr(void) {
    uint32_t edge_us = brd_capture_time_us();
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
    g_processed_rpm_generation = g_rpm_generation;
    g_input_capture = true;
    portEXIT_CRITICAL(&g_input_mux);
}

static void reset_measurement(bool loaded) {
    /* [BRD_BBP 新增] 只重置本次曲線，不清除已發布歷史。 */
    brd_bbp_capture_reset();
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
    g_rearm_wait = false;
    g_capture_tainted = false;
    g_state = (loaded || !measurement_uses_load()) ? BRD_STATE_LOADED_READY : BRD_STATE_WAIT_LOAD;
    g_show_max = false;
    g_display_max = 0U;
    g_max_lock = false;
    g_max_frame_seen = false;
    g_display_generation++;
    update_modification_window();
}

static void finish_measurement(void) {
    if (g_state != BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }
    if (!g_has_valid_rpm) {
        reset_measurement(false);
        return;
    }

    /* [修改] 已知事件遺失時，不把不完整 MAX/曲線發布成有效結果。 */
    if (g_capture_tainted) {
        brd_bbp_capture_abort();
        increment_counter(g_diagnostics.invalid_measurements);
        g_show_max = false;
        g_max_lock = false;
    } else {
        brd_bbp_capture_finish();
        g_display_max = g_max_rpm;
        g_show_max = true;
        g_max_lock = true;
    }
    g_display_generation++;
    g_max_frame_seen = false;
    g_max_lock_start_ms = millis();
    g_state = BRD_STATE_WAIT_LOAD;
    g_current_rpm = 0U;
    g_period_valid[LOW] = g_period_valid[HIGH] = false;
    g_quiet_expired = true;
    /* [刪減] 結束量測不再 disable_input_capture，尾段事件仍持續取走。 */
    g_rearm_wait = true;
    g_rearm_seen_low = g_load_stable != LOAD_ACTIVE_LEVEL;
    g_rearm_activity_us = g_last_activity_us;
    g_rpm_reference_level = -1;
    update_modification_window();
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
        static_cast<uint32_t>(g_max_rpm) * brd_settings_get().finish_percent) {
        g_launch_us = event_us;
        g_state = BRD_STATE_SPINNING_LAUNCHED;
        brd_bbp_capture_launch(event_us);
        increment_counter(g_diagnostics.launch_events);
        finish_measurement();
    }
}

static void process_rpm_edge(const input_edge_t &edge) {
    if (edge.generation != g_processed_rpm_generation) {
        /* [新增] 跨遺失區段只重建基準，不把兩圈以上間隔誤算成低 RPM。 */
        g_period_valid[LOW] = g_period_valid[HIGH] = false;
        g_processed_rpm_generation = edge.generation;
        if (g_state == BRD_STATE_SPINNING_LOADED) { g_capture_tainted = true; }
    }
    if (g_rearm_wait) { g_rearm_activity_us = edge.time_us; return; }
    /* [修改] LOAD 模式在未完成裝載啟動前完全忽略 RPM。 */
    if (g_state == BRD_STATE_WAIT_LOAD) {
        if (measurement_uses_load()) {
            return;
        }
        g_state = BRD_STATE_LOADED_READY;
    }
    if (g_state == BRD_STATE_LOADED_READY) {
        if (g_show_max || g_capture_tainted) {
            reset_measurement(true);
        }
        g_state = BRD_STATE_SPINNING_LOADED;
        update_modification_window();
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

    /*
        [修改] 每一筆「參考極性同極性週期」產生的非 0 RPM 都送入 Session。
        Profile 自己只保存最早 32 點；即使 Profile 已滿，Session 仍會以後續參考週期更新 representative/MAX。
        因此 BLE 取線與 MAX 完全獨立，另一極性不參與 RPM/MAX/曲線計算。
    */
    brd_bbp_capture_period(period_us, edge_us, true);
    if (g_current_rpm > g_max_rpm) {
        g_max_rpm = g_current_rpm;
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
    if (g_rearm_wait) {
        if (g_load_stable != LOAD_ACTIVE_LEVEL) { g_rearm_seen_low = true; }
        else if (g_rearm_seen_low) { reset_measurement(true); }
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
    INPUT_TIMER_AUTO_RESET,
    INPUT_TIMER_REARM
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
    for (uint8_t pass = 0U; pass < 6U; pass++) {
        input_timer_t timer = INPUT_TIMER_NONE;
        uint32_t deadline = 0UL;
        if (g_rearm_wait && !suppress_rpm_timeouts) {
            consider_timer(g_rearm_activity_us + RPM_ZERO_TIMEOUT_MS * 1000UL,
                           INPUT_TIMER_REARM, until_us, include_equal_timeouts, timer, deadline);
        }
        if (measurement_uses_load()) {
            if (g_load_raw != g_load_stable) {
                consider_timer(static_cast<uint32_t>(g_load_candidate_us + LOAD_IR_DEBOUNCE_US),
                               INPUT_TIMER_LOAD, until_us, true, timer, deadline);
            }
            if (!suppress_rpm_timeouts && !g_rearm_wait) {
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
        } else if (!suppress_rpm_timeouts && !g_rearm_wait && g_state == BRD_STATE_SPINNING_LOADED) {
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
        if (timer == INPUT_TIMER_REARM) {
            g_rearm_wait = false;
            brd_bbp_capture_reset();
            g_state = (!measurement_uses_load() || g_load_stable == LOAD_ACTIVE_LEVEL) ?
                      BRD_STATE_LOADED_READY : BRD_STATE_WAIT_LOAD;
            g_period_valid[LOW] = g_period_valid[HIGH] = false;
            g_rpm_reference_level = -1;
            g_quiet_expired = true;
            g_has_valid_rpm = false;
            g_current_rpm = g_max_rpm = 0U;
            g_auto_launch_candidate = false;
        } else if (timer == INPUT_TIMER_LOAD) {
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
                            bool &rpm_overflow, bool &load_overflow, uint32_t &activity_us) {
    rpm_count = 0U;
    load_count = 0U;
    portENTER_CRITICAL(&g_input_mux);
    rpm_overflow = g_rpm_overflow;
    load_overflow = measurement_uses_load() ? g_load_overflow : false;
    g_rpm_overflow = false;
    g_load_overflow = false;
    /* [修改] 先封存本輪 head；每 16 筆釋放 critical section，ISR 可繼續擷取。 */
    uint16_t rpm_end = g_rpm_head;
    uint16_t load_end = g_load_head;
    now_us = micros();
    activity_us = g_diagnostics.last_rpm_isr_us;
    bool missing_load = false;
    int missing_level = LOW;
    if (measurement_uses_load() && !load_overflow) {
        missing_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
        missing_load = missing_level != g_isr_load_level;
        if (missing_load) { g_isr_load_level = missing_level; }
    }
    while (g_rpm_tail != rpm_end && rpm_count < RPM_ISR_QUEUE_SIZE) {
        for (uint16_t n = 0U; n < RPM_SNAPSHOT_CHUNK_SIZE && g_rpm_tail != rpm_end; n++) {
            g_rpm_batch[rpm_count].time_us = g_rpm_queue[g_rpm_tail].time_us;
            g_rpm_batch[rpm_count].level = g_rpm_queue[g_rpm_tail].level;
            g_rpm_batch[rpm_count].generation = g_rpm_queue[g_rpm_tail].generation;
            rpm_count++;
            g_rpm_tail = (uint16_t)((g_rpm_tail + 1U) % RPM_ISR_QUEUE_SIZE);
        }
        portEXIT_CRITICAL(&g_input_mux);
        portENTER_CRITICAL(&g_input_mux);
    }
    if (measurement_uses_load()) {
        while (g_load_tail != load_end && load_count < LOAD_ISR_QUEUE_SIZE - 1U) {
            g_load_batch[load_count].time_us = g_load_queue[g_load_tail].time_us;
            g_load_batch[load_count].level = g_load_queue[g_load_tail].level;
            load_count++;
            g_load_tail = (uint16_t)((g_load_tail + 1U) % LOAD_ISR_QUEUE_SIZE);
        }
    } else {
        /* [新增] AUTO 不累積、不處理 LOAD 事件。 */
        g_load_tail = g_load_head;
    }
    if (missing_load) {
        g_load_batch[load_count].time_us = now_us;
        g_load_batch[load_count].level = missing_level;
        load_count++;
    }
    portEXIT_CRITICAL(&g_input_mux);
}

void brd_measurement_begin(void) {
    /* [V0.12 新增] 允許主 loop 重複呼叫；ADC 故障恢復時才重新建立量測。 */
    if (g_measurement_enabled) {
        portENTER_CRITICAL(&g_input_mux);
        if (!g_input_capture && !g_maintenance_paused) {
            g_input_capture = true;
            increment_counter(g_diagnostics.capture_recoveries);
        }
        portEXIT_CRITICAL(&g_input_mux);
        return;
    }
    g_measurement_stopped = false;
    reset_measurement(!measurement_uses_load());
    g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    restart_input_capture();
    g_measurement_interrupts_attached = brd_capture_attach_rpm(rpm_ir_isr);
    if (!g_measurement_interrupts_attached) {
        disable_input_capture(); g_capture_fault = true;
        increment_counter(g_diagnostics.capture_init_failures); return;
    }
    g_capture_fault = false;
    if (measurement_uses_load()) {
        g_load_interrupt_attached = brd_capture_attach_load(load_ir_isr);
        if (!g_load_interrupt_attached) {
            brd_capture_detach_rpm(); g_measurement_interrupts_attached = false;
            disable_input_capture(); g_capture_fault = true;
            increment_counter(g_diagnostics.capture_init_failures); return;
        }
    } else {
        g_load_interrupt_attached = false;
    }
    g_measurement_enabled = true;
    update_modification_window();
}

void brd_measurement_stop(void) {
    if (g_measurement_stopped) {
        return;
    }
    g_measurement_stopped = true;
    g_measurement_enabled = false;
    g_settings_paused = g_maintenance_paused = false;
    disable_input_capture();
    if (g_measurement_interrupts_attached) {
        brd_capture_detach_rpm();
        g_measurement_interrupts_attached = false;
    }
    if (g_load_interrupt_attached) {
        brd_capture_detach_load();
        g_load_interrupt_attached = false;
    }
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
    if (g_maintenance_paused) { return; }
    /* [修改] HOLD 到期只變更顯示，不清空事件或停止量測。 */
    if (g_max_lock && (uint32_t)(millis() - g_max_lock_start_ms) >= OLED_MAX_HOLD_MS) {
        g_max_lock = false;
    }

    uint16_t rpm_count;
    uint16_t load_count;
    uint32_t now_us;
    bool rpm_overflow;
    bool load_overflow;
    uint32_t activity_us;
    snapshot_inputs(rpm_count, load_count, now_us, rpm_overflow, load_overflow, activity_us);
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
    while (rpm_index < rpm_count || load_index < load_count) {
        bool use_rpm = rpm_index < rpm_count &&
            (load_index >= load_count ||
             !time_before(g_load_batch[load_index].time_us, g_rpm_batch[rpm_index].time_us));
        uint32_t event_us = use_rpm ? g_rpm_batch[rpm_index].time_us : g_load_batch[load_index].time_us;
        advance_input_time(event_us, false, rpm_overflow);
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
    if (rpm_overflow) {
        /* [修正] 遺失尾段事件仍表示活動中，不得以舊 FIFO 尾端誤判安靜。 */
        if (g_rearm_wait) { g_rearm_activity_us = activity_us; }
        if (g_state == BRD_STATE_SPINNING_LOADED) { g_capture_tainted = true; }
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
    advance_input_time(now_us, true, false);
    update_modification_window();
}

brd_display_t brd_measurement_get_display(void) {
    brd_display_t display;
    /* [修改] AUTO 的 BLE 裝載旗標直接跟隨即時 RPM；OLED 文字另由 AUTO READY 固定顯示。 */
    display.loaded = measurement_uses_load() ?
                     (g_load_stable == LOAD_ACTIVE_LEVEL) : (g_current_rpm != 0U);
    display.show_max = g_show_max;
    display.value = g_show_max ? g_display_max : g_current_rpm;
    display.generation = g_display_generation;
    display.hold_active = g_show_max && g_max_lock;
    display.input_fault = g_capture_fault;
    display.sample_loss = g_capture_tainted;
    return display;
}

brd_measurement_diagnostics_t brd_measurement_get_diagnostics(void) {
    portENTER_CRITICAL(&g_input_mux);
    brd_measurement_diagnostics_t result = g_diagnostics;
    portEXIT_CRITICAL(&g_input_mux);
    return result;
}

void brd_measurement_max_frame_presented(uint32_t generation) {
    if (!g_measurement_enabled || !g_show_max ||
        generation != g_display_generation || g_max_frame_seen) {
        return;
    }
    g_max_frame_seen = true;
    /* [刪減] OLED 回報不再控制輸入捕捉，也不讓過期 HOLD 復活。 */
    if (g_max_lock) { g_max_lock_start_ms = millis(); }
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
        /* [修改] 一般 NVS 只取得設定交易，不停止 IRAM ISR。 */
        g_settings_paused = true;
    }
    portEXIT_CRITICAL(&g_input_mux);
    return allowed;
}
/* [新增] 只在 ISR 尚未排入新事件的原子窗口套用設定。 */
bool brd_measurement_apply_settings(void (*apply)(void)) {
    portENTER_CRITICAL(&g_input_mux);
    bool allowed = g_settings_paused && !g_maintenance_paused && g_modification_window &&
                   !g_rpm_overflow && !g_load_overflow &&
                   g_rpm_head == g_rpm_tail && g_load_head == g_load_tail;
    if (allowed) {
        bool used_load = measurement_uses_load();
        apply();
        /* [修正] 同模式僅更新參數，不重設已完成的 LOAD 去抖及 READY。 */
        if (used_load != measurement_uses_load()) {
            g_period_valid[LOW] = g_period_valid[HIGH] = false;
            g_rpm_reference_level = -1;
            g_quiet_expired = true;
            g_has_valid_rpm = false;
            g_current_rpm = g_max_rpm = 0U;
            g_auto_launch_candidate = false;
            g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
            g_isr_load_level = measurement_uses_load() ? (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U) : LOAD_ACTIVE_LEVEL;
            g_load_raw = g_isr_load_level;
            g_load_candidate_us = micros();
            g_state = measurement_uses_load() ? BRD_STATE_WAIT_LOAD : BRD_STATE_LOADED_READY;
        }
        g_settings_paused = false;
    }
    portEXIT_CRITICAL(&g_input_mux);
    if (allowed && g_measurement_enabled) {
        if (g_load_interrupt_attached && !measurement_uses_load()) {
            brd_capture_detach_load(); g_load_interrupt_attached = false;
        } else if (!g_load_interrupt_attached && measurement_uses_load()) {
            g_load_interrupt_attached = brd_capture_attach_load(load_ir_isr);
            if (!g_load_interrupt_attached) { g_capture_fault = true; }
        }
    }
    update_modification_window();
    return allowed;
}
void brd_measurement_resume_after_settings(bool applied) {
    (void)applied;
    /* [刪減] 不再 restart_input_capture，避免清除 Flash 期間的脈衝。 */
    portENTER_CRITICAL(&g_input_mux);
    if (!g_maintenance_paused) { g_settings_paused = false; }
    portEXIT_CRITICAL(&g_input_mux);
    update_modification_window();
}
bool brd_measurement_pause_for_ota(void) {
    portENTER_CRITICAL(&g_input_mux);
    bool allowed = g_modification_window && !g_settings_paused && !g_rpm_overflow && !g_load_overflow &&
                   g_rpm_head == g_rpm_tail && g_load_head == g_load_tail;
    if (allowed) {
        g_settings_paused = g_maintenance_paused = true;
        g_input_capture = false;
    }
    portEXIT_CRITICAL(&g_input_mux);
    return allowed;
}
void brd_measurement_resume_after_ota(void) {
    if (!g_maintenance_paused) { return; }
    g_maintenance_paused = g_settings_paused = false;
    g_period_valid[LOW] = g_period_valid[HIGH] = false;
    g_rpm_reference_level = -1;
    g_quiet_expired = true;
    if (g_measurement_enabled) { restart_input_capture(); }
    update_modification_window();
}
bool brd_measurement_background_ready(void) {
    portENTER_CRITICAL(&g_input_mux);
    bool ready = !g_rpm_overflow && !g_load_overflow &&
                 g_rpm_head == g_rpm_tail && g_load_head == g_load_tail;
    portEXIT_CRITICAL(&g_input_mux);
    return ready;
}
