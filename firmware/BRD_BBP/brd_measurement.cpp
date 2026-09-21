/*
    檔案位置: BRD_BBP/brd_measurement.cpp
    [V0.12 修改] LOAD / RPM 事件依時間戳記合併處理，去抖使用事件時間。
    [V0.12 新增] 有界 LOAD 佇列、固定大小快照、溢位診斷與重新同步。
    [V0.12 刪減] LOAD 僅保存最後一次電位及 sequence 的處理方式。
    [V0.12 修改] 發射後門檻使用 cfg 的 MAX 20%。
    [保留] 不加入相鄰 RPM 週期合理性檢查或候選峰值濾波。
    [保留] HOLD 2.5 秒、OLED 完整畫面回報、無脈衝 300 ms 結算。
    [R2 修改] ISR 保存時間及電位，兩種邊沿分別量測整圈，每圈更新兩次 RPM。
    [R3 新增] 曲線以裝載後首個 RPM 邊沿為固定參考，每圈保存一筆。
    ISR 只記錄事件；所有狀態機與 OLED 仍在主 loop 執行。
*/
#include <soc/gpio_struct.h>

#include "brd_config.h"
#include "brd_bbp.h"
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
static volatile int g_isr_load_level = LOW;

/* [V0.12 新增] 固定 RAM 快照，不使用 heap，也不把陣列放在主迴圈堆疊。 */
static input_edge_t g_rpm_batch[RPM_ISR_QUEUE_SIZE];
static input_edge_t g_load_batch[LOAD_ISR_QUEUE_SIZE];
static brd_measurement_diagnostics_t g_diagnostics = {};
static bool g_measurement_enabled = false;
static bool g_measurement_stopped = false;
static bool g_measurement_interrupts_attached = false;
static brd_state_t g_state = BRD_STATE_WAIT_LOAD;
static int g_load_raw = LOW;
static int g_load_stable = LOW;
static uint32_t g_load_candidate_us = 0UL;
static bool g_load_candidate_had_spin = false;
/* [R2 新增] LOW 為下降沿，HIGH 為上升沿；分開保存整圈時間基準。 */
static bool g_period_valid[2] = {false, false};
/* [R3 新增] -1 表示尚未選定；首個觸發的 HIGH/LOW 決定本次曲線參考。 */
static int g_curve_reference_level = -1;
static bool g_quiet_expired = true;
static uint32_t g_last_edge_us[2] = {0UL, 0UL};
static uint32_t g_last_activity_us = 0UL;
static uint32_t g_launch_us = 0UL;
static bool g_has_valid_rpm = false;
static uint16_t g_current_rpm = 0U;
static uint16_t g_max_rpm = 0U;
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

static void IRAM_ATTR rpm_ir_isr(void) {
    uint32_t edge_us = micros();
    int level = (int)((GPIO.in.val >> RPM_IR_GPIO) & 1U);
    portENTER_CRITICAL_ISR(&g_input_mux);
    if (g_input_capture) {
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

static void restart_load_debounce(void) {
    /* [V0.12 修改] 開始、故障重同步及 HOLD 解鎖時，丟棄所有歷史事件。 */
    portENTER_CRITICAL(&g_input_mux);
    g_rpm_head = 0U;
    g_rpm_tail = 0U;
    g_load_head = 0U;
    g_load_tail = 0U;
    g_rpm_overflow = false;
    g_load_overflow = false;
    g_isr_load_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
    g_load_raw = g_isr_load_level;
    g_load_candidate_us = micros();
    g_load_candidate_had_spin = false;
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
    g_curve_reference_level = -1;
    g_quiet_expired = true;
    g_has_valid_rpm = false;
    g_current_rpm = 0U;
    g_max_rpm = 0U;
    g_state = loaded ? BRD_STATE_LOADED_READY : BRD_STATE_WAIT_LOAD;
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

    /* [BRD_BBP 新增] 凍結完整曲線，發布及 Flash 留給主 loop。 */
    brd_bbp_capture_finish();
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

static void check_finish_threshold(void) {
    if (g_state != BRD_STATE_SPINNING_LAUNCHED || !g_has_valid_rpm ||
        g_current_rpm == 0U || g_max_rpm == 0U) {
        return;
    }
    uint32_t threshold = ((uint32_t)g_max_rpm * POST_LAUNCH_FINISH_PERCENT + 99UL) / 100UL;
    if (g_current_rpm <= threshold) {
        finish_measurement();
    }
}

static void process_rpm_edge(const input_edge_t &edge) {
    if (g_state == BRD_STATE_WAIT_LOAD) {
        return;
    }
    if (g_state == BRD_STATE_LOADED_READY) {
        g_state = BRD_STATE_SPINNING_LOADED;
    }
    const uint32_t edge_us = edge.time_us;
    const uint8_t polarity = static_cast<uint8_t>(edge.level);
    if (g_curve_reference_level < 0) {
        /* [R3 新增] 以首個實際觸發選擇，不使用初始電位或首筆有效 RPM。 */
        g_curve_reference_level = edge.level;
    }
    if (!g_period_valid[polarity]) {
        /* [R2 新增] 每類第一個邊沿只建立基準，尚未有整圈數值。 */
        g_last_edge_us[polarity] = edge_us;
        g_last_activity_us = edge_us;
        g_period_valid[polarity] = true;
        g_quiet_expired = false;
        return;
    }

    /* [R2 修改] 上升到上升、下降到下降，完全不使用相鄰異類邊沿的脈寬。 */
    uint32_t period_us = static_cast<uint32_t>(edge_us - g_last_edge_us[polarity]);
    if (period_us < RPM_MIN_PERIOD_US) {
        return;
    }
    if (period_us > RPM_MAX_PERIOD_US) {
        g_last_edge_us[polarity] = edge_us;
        g_last_activity_us = edge_us;
        g_current_rpm = 0U;
        g_quiet_expired = false;
        return;
    }
    uint32_t rpm = 60000000UL / period_us / PULSES_PER_REV;
    if (rpm > RPM_VALID_MAX) {
        return;
    }

    g_last_edge_us[polarity] = edge_us;
    g_last_activity_us = edge_us;
    g_quiet_expired = false;
    g_current_rpm = static_cast<uint16_t>(rpm);
    g_has_valid_rpm = true;
    /* [R3 修改] 兩類邊沿都更新代表 MAX，只有選定參考邊沿加入曲線。
       [R3 刪減] 不再將另一類邊沿的重疊整圈週期也寫入 Profile。 */
    brd_bbp_capture_period(period_us, edge_us, edge.level == g_curve_reference_level);
    if (g_current_rpm > g_max_rpm) {
        g_max_rpm = g_current_rpm;
    }
    check_finish_threshold();
}

// [API V1.1 修改] confirmed_us 是 LOAD 去抖到期時間。
static void handle_load_change(uint32_t confirmed_us) {
    g_load_stable = g_load_raw;
    increment_counter(g_diagnostics.load_stable_transitions);
    if (g_load_stable == LOAD_ACTIVE_LEVEL) {
        reset_measurement(true);
    } else if (g_state == BRD_STATE_SPINNING_LOADED) {
        /* [V0.12 修改] 在原 LOW 邊沿前未開始轉動，不把卸載後脈衝算成發射。 */
        if (!g_load_candidate_had_spin) {
            reset_measurement(false);
            return;
        }
        g_launch_us = g_load_candidate_us;
        g_state = BRD_STATE_SPINNING_LAUNCHED;
        /* [BRD_BBP 新增] 經去抖確認且曾轉動，才承認發射。 */
        brd_bbp_capture_launch(confirmed_us);
        increment_counter(g_diagnostics.launch_events);
        check_finish_threshold();
        /* [保留] 已靜止超過歸零期限後卸載，也能完成既有有效結果。 */
        if (g_quiet_expired && g_has_valid_rpm) {
            finish_measurement();
        }
    } else if (g_state == BRD_STATE_LOADED_READY) {
        reset_measurement(false);
    }
}

enum input_timer_t {
    INPUT_TIMER_NONE,
    INPUT_TIMER_LOAD,
    INPUT_TIMER_ZERO,
    INPUT_TIMER_IDLE,
    INPUT_TIMER_EMPTY_LAUNCH
};

static void consider_timer(uint32_t deadline, input_timer_t type, uint32_t until_us,
                           bool include_equal, input_timer_t &selected, uint32_t &selected_us) {
    bool due = time_before(deadline, until_us) || (include_equal && deadline == until_us);
    if (due && (selected == INPUT_TIMER_NONE || time_before(deadline, selected_us))) {
        selected = type;
        selected_us = deadline;
    }
}

static void advance_input_time(uint32_t until_us, bool include_equal_timeouts) {
    /*
        [V0.12 新增] 去抖到期與量測 timeout 同樣按事件時間排序。
        每段最多處理一次 LOAD、一次歸零及一次重置，四輪是固定上限。
        同時刻的實際 RPM 先於歸零 timeout；滿去抖時間則承認前一狀態。
    */
    for (uint8_t pass = 0U; pass < 4U && !g_max_lock; pass++) {
        input_timer_t timer = INPUT_TIMER_NONE;
        uint32_t deadline = 0UL;
        if (g_load_raw != g_load_stable) {
            consider_timer((uint32_t)(g_load_candidate_us + LOAD_IR_DEBOUNCE_US),
                           INPUT_TIMER_LOAD, until_us, true, timer, deadline);
        }
        bool spinning = g_state == BRD_STATE_SPINNING_LOADED ||
                        g_state == BRD_STATE_SPINNING_LAUNCHED;
        if (spinning && !g_quiet_expired) {
            consider_timer((uint32_t)(g_last_activity_us + RPM_ZERO_TIMEOUT_MS * 1000UL),
                           INPUT_TIMER_ZERO, until_us, include_equal_timeouts, timer, deadline);
        }
        if (g_state == BRD_STATE_SPINNING_LOADED) {
            consider_timer((uint32_t)(g_last_activity_us + PRELAUNCH_IDLE_RESET_MS * 1000UL),
                           INPUT_TIMER_IDLE, until_us, include_equal_timeouts, timer, deadline);
        }
        if (g_state == BRD_STATE_SPINNING_LAUNCHED && !g_has_valid_rpm) {
            consider_timer((uint32_t)(g_launch_us + POST_LAUNCH_NO_RPM_TIMEOUT_MS * 1000UL),
                           INPUT_TIMER_EMPTY_LAUNCH, until_us, include_equal_timeouts, timer, deadline);
        }

        if (timer == INPUT_TIMER_NONE) {
            return;
        }
        if (timer == INPUT_TIMER_LOAD) {
            handle_load_change(deadline);
        } else if (timer == INPUT_TIMER_ZERO) {
            g_current_rpm = 0U;
            g_period_valid[LOW] = g_period_valid[HIGH] = false;
            g_quiet_expired = true;
            if (g_state == BRD_STATE_SPINNING_LAUNCHED && g_has_valid_rpm) {
                finish_measurement();
            }
        } else if (timer == INPUT_TIMER_IDLE) {
            reset_measurement(true);
        } else {
            finish_measurement();
        }
    }
}

static void snapshot_inputs(uint16_t &rpm_count, uint16_t &load_count, uint32_t &now_us,
                            bool &rpm_overflow, bool &load_overflow) {
    rpm_count = 0U;
    load_count = 0U;
    portENTER_CRITICAL(&g_input_mux);
    rpm_overflow = g_rpm_overflow;
    load_overflow = g_load_overflow;
    g_rpm_overflow = false;
    g_load_overflow = false;
    while (g_rpm_tail != g_rpm_head && rpm_count < RPM_ISR_QUEUE_SIZE) {
        g_rpm_batch[rpm_count].time_us = g_rpm_queue[g_rpm_tail].time_us;
        g_rpm_batch[rpm_count].level = g_rpm_queue[g_rpm_tail].level;
        rpm_count++;
        g_rpm_tail = (uint16_t)((g_rpm_tail + 1U) % RPM_ISR_QUEUE_SIZE);
    }
    while (g_load_tail != g_load_head && load_count < LOAD_ISR_QUEUE_SIZE - 1U) {
        g_load_batch[load_count].time_us = g_load_queue[g_load_tail].time_us;
        g_load_batch[load_count].level = g_load_queue[g_load_tail].level;
        load_count++;
        g_load_tail = (uint16_t)((g_load_tail + 1U) % LOAD_ISR_QUEUE_SIZE);
    }
    now_us = micros();
    int actual_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
    if (actual_level != g_isr_load_level && !load_overflow) {
        /* [V0.12 新增] 補捉尚未送達的 ISR；補捉時間起重新完整去抖。 */
        g_load_batch[load_count].time_us = now_us;
        g_load_batch[load_count].level = actual_level;
        load_count++;
        g_isr_load_level = actual_level;
    }
    portEXIT_CRITICAL(&g_input_mux);
}

void brd_measurement_begin(void) {
    /* [V0.12 新增] 允許主 loop 重複呼叫；ADC 故障恢復時才重新建立量測。 */
    if (g_measurement_enabled) {
        return;
    }
    g_measurement_stopped = false;
    reset_measurement(false);
    g_load_stable = LOW;
    restart_load_debounce();
    attachInterrupt(digitalPinToInterrupt(RPM_IR_GPIO), rpm_ir_isr, RPM_IR_TRIGGER_EDGE);
    attachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO), load_ir_isr, LOAD_IR_TRIGGER_EDGE);
    g_measurement_interrupts_attached = true;
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
        detachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO));
        g_measurement_interrupts_attached = false;
    }
    reset_measurement(false);
    g_load_raw = LOW;
    g_load_stable = LOW;
    /* [BRD_BBP 新增] 低電/ADC 故障中斷的一發不發布。 */
    brd_bbp_capture_abort();
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
        g_load_stable = LOW;
        restart_load_debounce();
        return;
    }

    uint16_t rpm_count;
    uint16_t load_count;
    uint32_t now_us;
    bool rpm_overflow;
    bool load_overflow;
    snapshot_inputs(rpm_count, load_count, now_us, rpm_overflow, load_overflow);
    if (rpm_overflow) {
        increment_counter(g_diagnostics.rpm_queue_overflows);
        /* [BRD_BBP 新增] 已缺圈，不能發布非連續曲線。 */
        brd_bbp_capture_abort();
        g_period_valid[LOW] = g_period_valid[HIGH] = false;
        g_current_rpm = 0U;
        rpm_count = 0U;
    }
    if (load_overflow) {
        /* [V0.12 新增] 缺失裝載歷史時作廢進行中的量測，不推測發射事件。 */
        increment_counter(g_diagnostics.load_queue_overflows);
        if (g_state != BRD_STATE_WAIT_LOAD) {
            reset_measurement(false);
        }
        g_load_stable = LOW;
        restart_load_debounce();
        return;
    }

    uint16_t rpm_index = 0U;
    uint16_t load_index = 0U;
    while ((rpm_index < rpm_count || load_index < load_count) && !g_max_lock) {
        bool use_rpm = rpm_index < rpm_count &&
            (load_index >= load_count ||
             !time_before(g_load_batch[load_index].time_us, g_rpm_batch[rpm_index].time_us));
        uint32_t event_us = use_rpm ? g_rpm_batch[rpm_index].time_us : g_load_batch[load_index].time_us;
        advance_input_time(event_us, false);
        if (g_max_lock) {
            break;
        }
        if (use_rpm) {
            process_rpm_edge(g_rpm_batch[rpm_index++]);
        } else {
            const input_edge_t &edge = g_load_batch[load_index++];
            g_load_raw = edge.level;
            g_load_candidate_us = edge.time_us;
            g_load_candidate_had_spin = g_state == BRD_STATE_SPINNING_LOADED;
        }
    }
    if (!g_max_lock) {
        advance_input_time(now_us, true);
    }
}

brd_display_t brd_measurement_get_display(void) {
    brd_display_t display;
    display.loaded = g_load_stable == LOAD_ACTIVE_LEVEL;
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

/* [BRD_BBP 新增] WAIT_LOAD 與 HOLD 才處理非即時工作。 */
bool brd_measurement_storage_allowed(void) {
    return !g_measurement_enabled || g_state == BRD_STATE_WAIT_LOAD;
}
