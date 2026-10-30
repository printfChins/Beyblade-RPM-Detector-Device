/*
    檔案位置: BRD_BPX/brd_measurement.cpp
    [V0.12 修改] LOAD / RPM 事件依時間戳記合併處理，去抖使用事件時間。
    [V0.12 新增] 有界 LOAD 佇列、固定大小快照、溢位診斷與重新同步。
    [V0.12 刪減] LOAD 僅保存最後一次電位及 sequence 的處理方式。
    [V0.12 修改] 發射後門檻使用 cfg 的 MAX 設定百分比。
    [保留] 不加入相鄰 RPM 週期合理性檢查或候選峰值濾波。
    [V1.4.2 修改] LOAD HOLD 恢復 2.5 秒（AUTO 原本 2.5 秒）；HOLD 不接收 LOAD/RPM 事件，發射結果於 HOLD 階段發布。
    [修改] RPM 使用 CHANGE；每次量測以第一個實際觸發邊沿固定參考極性。
    [修改] 先正緣則正緣到正緣計算 RPM；先負緣則負緣到負緣計算 RPM，中途不切換。
    [修改] LOAD 只負責裝載後歸零/啟動量測；卸載不參與發射成功判定。
    [修改] 第一筆非 0 RPM 起即保存曲線，BLE Profile 只保留最早 32 點。
    [修改] MAX 與 32 點曲線獨立；32 點之後仍持續更新 MAX。
    [V1.2.3 修改] LOAD 發射完成由即時 SP <= MAX × 結束百分比判定；AUTO 保留原嚴格小於規則。
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
    g_display_max = g_max_rpm;
    g_show_max = true;
    g_display_generation++;
    g_max_lock = true;
    g_load_release_required = measurement_uses_load();
    g_max_frame_seen = false;
    g_max_lock_start_ms = millis();
    g_state = BRD_STATE_HOLD;
    g_current_rpm = 0U;
    g_period_valid[LOW] = g_period_valid[HIGH] = false;
    g_quiet_expired = true;
    update_modification_window();
    /* [V1.2.3 修改] 只有 HOLD 已成立，才正式封存本次 BLE SP/MAX/曲線。 */
    brd_bbp_capture_finish();
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
    /*
        [V1.2.3 修改] LOAD 在「等於」結束百分比時也完成。
        範例 MAX 10000 / EndPercent 30：降到 3000 時即進入 HOLD。
        [保留] AUTO 原先的嚴格小於邏輯，不擅自改動 AUTO 模式。
    */
    const uint32_t current_scaled = static_cast<uint32_t>(g_current_rpm) * 100UL;
    const uint32_t finish_scaled = static_cast<uint32_t>(g_max_rpm) *
                                    brd_settings_get().finish_percent;
    const bool reached_finish = measurement_uses_load() ?
                                (current_scaled <= finish_scaled) :
                                (current_scaled < finish_scaled);
    if (reached_finish) {
        g_launch_us = event_us;
        g_state = BRD_STATE_SPINNING_LAUNCHED;
        brd_bbp_capture_launch(event_us);
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
    update_modification_window();
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
    /* [BRD_BBP 新增] 低電/ADC 故障中斷的一發不發布。 */
    brd_bbp_capture_abort();
}

void brd_measurement_update(void) {
    if (!g_measurement_enabled) {
        return;
    }
    if (g_max_lock) {
        /* [V1.4.2 修改] LOAD HOLD 恢復原 V1.2 的 2.5 秒；AUTO 仍為 2.5 秒。 */
        const uint32_t hold_interval_ms = measurement_uses_load() ?
                                         LOAD_MAX_HOLD_MS : OLED_MAX_HOLD_MS;
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
        g_rpm_reference_level = -1;
        if (measurement_uses_load()) {
            g_load_release_required = true;
        } else {
            /* [新增] AUTO HOLD 結束後回邏輯 READY；MAX 保留到下一次 RPM 啟動。 */
            g_state = BRD_STATE_LOADED_READY;
            g_current_rpm = 0U;
            g_has_valid_rpm = false;
            g_period_valid[LOW] = g_period_valid[HIGH] = false;
            g_quiet_expired = true;
            g_auto_launch_candidate = false;
        }
        update_modification_window();
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
        /* [新增] 模式切換更新 LOAD 中斷，但不呼叫 stop/reset_capture 清除已完成曲線。 */
        if (g_load_interrupt_attached && !measurement_uses_load()) {
            detachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO));
            g_load_interrupt_attached = false;
        } else if (!g_load_interrupt_attached && measurement_uses_load()) {
            attachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO), load_ir_isr, LOAD_IR_TRIGGER_EDGE);
            g_load_interrupt_attached = true;
        }
        g_period_valid[LOW] = g_period_valid[HIGH] = false;
        g_rpm_reference_level = -1;
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
    if (g_measurement_enabled && !g_max_lock) { restart_input_capture(); }
}
