/*
    檔案位置: BRD_OLED/brd_measurement.cpp
    [V0.10 修改] 保留 RPM / LOAD ISR、即時 RPM、發射後 50% MAX 結束判斷。
    [V0.10 刪減] BLE、ACK、Session、CRC、曲線陣列與 LAUNCH RPM 歷史陣列。
    [V0.10 新增] MAX 自鎖、LOAD 完整去抖、溢位後重新建立 RPM 週期。
    [V0.10 新增] 低電時停用 RPM / LOAD 中斷，清除事件並停止所有量測處理。
    [V0.10 新增] 將實際 MAX 自鎖狀態提供給 OLED 顯示 HOLD。
    狀態機與 OLED 均由主 loop 執行；ISR 只記錄事件，不操作顯示器。
*/
#include <soc/gpio_struct.h>

#include "brd_config.h"
#include "brd_measurement.h"

static portMUX_TYPE g_input_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t g_rpm_queue[RPM_ISR_QUEUE_SIZE];
static volatile uint16_t g_rpm_head = 0U;
static volatile uint16_t g_rpm_tail = 0U;
static volatile bool g_rpm_overflow = false;
static volatile bool g_rpm_capture = false;
static volatile bool g_load_capture = true;
static volatile uint32_t g_load_edge_us = 0UL;
static volatile uint32_t g_load_sequence = 0UL;
static volatile int g_load_level = LOW;
static bool g_measurement_enabled = false;
static bool g_measurement_stopped = false;
static bool g_measurement_interrupts_attached = false;

static brd_state_t g_state = BRD_STATE_WAIT_LOAD;
static int g_load_raw = LOW;
static int g_load_stable = LOW;
static uint32_t g_load_candidate_us = 0UL;
static uint32_t g_load_seen_sequence = 0UL;
static bool g_period_valid = false;
static uint32_t g_last_edge_us = 0UL;
static uint32_t g_first_edge_us = 0UL;
static uint32_t g_first_edge_ms = 0UL;
static uint32_t g_last_activity_us = 0UL;
static uint32_t g_launch_ms = 0UL;
static bool g_has_valid_rpm = false;
static uint16_t g_current_rpm = 0U;
static uint16_t g_max_rpm = 0U;
static bool g_show_max = false;
static uint16_t g_display_max = 0U;
static uint32_t g_display_generation = 0UL;
static bool g_max_lock = false;
static bool g_max_frame_seen = false;
static uint32_t g_max_lock_start_ms = 0UL;

static void IRAM_ATTR rpm_ir_isr(void) {
    uint32_t edge_us = micros();

    portENTER_CRITICAL_ISR(&g_input_mux);
    if (g_rpm_capture) {
        uint16_t next = (uint16_t)((g_rpm_head + 1U) % RPM_ISR_QUEUE_SIZE);
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
    if (g_load_capture) {
        g_load_edge_us = edge_us;
        g_load_level = level;
        g_load_sequence++;
    }
    portEXIT_CRITICAL_ISR(&g_input_mux);
}

static void set_rpm_capture(bool enabled) {
    portENTER_CRITICAL(&g_input_mux);
    g_rpm_capture = enabled;
    g_rpm_head = 0U;
    g_rpm_tail = 0U;
    g_rpm_overflow = false;
    portEXIT_CRITICAL(&g_input_mux);
}

static void restart_load_debounce(void) {
    /* [V0.10 新增] 在同一臨界區重新取樣並丟棄自鎖期間的 LOAD 歷史。 */
    portENTER_CRITICAL(&g_input_mux);
    g_load_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
    g_load_edge_us = micros();
    g_load_sequence++;
    g_load_raw = g_load_level;
    g_load_candidate_us = g_load_edge_us;
    g_load_seen_sequence = g_load_sequence;
    g_load_capture = true;
    portEXIT_CRITICAL(&g_input_mux);
}

static void reset_measurement(bool loaded) {
    set_rpm_capture(false);
    g_period_valid = false;
    g_has_valid_rpm = false;
    g_current_rpm = 0U;
    g_max_rpm = 0U;
    g_state = loaded ? BRD_STATE_LOADED_READY : BRD_STATE_WAIT_LOAD;
    g_show_max = false;
    g_display_max = 0U;
    g_max_lock = false;
    g_max_frame_seen = false;
    g_display_generation++;

    if (loaded) {
        set_rpm_capture(true);
    }
}

static void finish_measurement(void) {
    if (g_state != BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }

    if (!g_has_valid_rpm) {
        reset_measurement(false);
        return;
    }

    /* [V0.10 修改] 一筆有效 RPM 即可顯示 MAX，不再套用 BLE 曲線點數門檻。 */
    g_display_max = g_max_rpm;
    g_show_max = true;
    g_display_generation++;
    g_max_lock = true;
    g_max_frame_seen = false;
    g_max_lock_start_ms = millis();
    g_state = BRD_STATE_WAIT_LOAD;
    g_current_rpm = 0U;
    g_period_valid = false;
    set_rpm_capture(false);

    portENTER_CRITICAL(&g_input_mux);
    g_load_capture = false;
    portEXIT_CRITICAL(&g_input_mux);
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

static void process_rpm_edge(uint32_t edge_us) {
    if (g_state == BRD_STATE_WAIT_LOAD) {
        return;
    }

    if (g_state == BRD_STATE_LOADED_READY) {
        /* [V0.10 新增] 不以裝載時間比較 micros，允許長時間裝載後才開始轉動。 */
        g_first_edge_us = edge_us;
        g_first_edge_ms = millis() - (uint32_t)(micros() - edge_us) / 1000UL;
        g_last_edge_us = edge_us;
        g_last_activity_us = edge_us;
        g_period_valid = true;
        g_state = BRD_STATE_SPINNING_LOADED;
        return;
    }

    if (!g_period_valid) {
        g_last_edge_us = edge_us;
        g_last_activity_us = edge_us;
        g_period_valid = true;
        return;
    }

    uint32_t period_us = (uint32_t)(edge_us - g_last_edge_us);
    if (period_us < RPM_MIN_PERIOD_US) {
        return;
    }
    if (period_us > RPM_MAX_PERIOD_US) {
        g_last_edge_us = edge_us;
        g_last_activity_us = edge_us;
        g_current_rpm = 0U;
        return;
    }

    uint32_t rpm = 60000000UL / period_us / PULSES_PER_REV;
    if (rpm > RPM_VALID_MAX) {
        return;
    }

    g_last_edge_us = edge_us;
    g_last_activity_us = edge_us;
    g_current_rpm = (uint16_t)rpm;
    g_has_valid_rpm = true;
    if (g_current_rpm > g_max_rpm) {
        g_max_rpm = g_current_rpm;
    }
    check_finish_threshold();
}

static void update_rpm_events(void) {
    /* [V0.10 新增] 每輪有明確上限，持續脈衝不會讓 LOAD 與 OLED 永遠排不到。 */
    for (uint16_t i = 0U; i < RPM_ISR_QUEUE_SIZE; i++) {
        uint32_t edge_us = 0UL;
        bool have_edge = false;
        bool overflow;

        portENTER_CRITICAL(&g_input_mux);
        overflow = g_rpm_overflow;
        if (overflow) {
            g_rpm_tail = g_rpm_head;
            g_rpm_overflow = false;
        } else if (g_rpm_tail != g_rpm_head) {
            edge_us = g_rpm_queue[g_rpm_tail];
            g_rpm_tail = (uint16_t)((g_rpm_tail + 1U) % RPM_ISR_QUEUE_SIZE);
            have_edge = true;
        }
        portEXIT_CRITICAL(&g_input_mux);

        if (overflow) {
            /* [V0.10 新增] 不以跨越遺失脈衝的間距計算假性低 RPM。 */
            g_period_valid = false;
            g_current_rpm = 0U;
            return;
        }
        if (!have_edge) {
            return;
        }
        process_rpm_edge(edge_us);
    }
}

static void handle_load_change(int new_level, uint32_t edge_us) {
    g_load_stable = new_level;

    if (new_level == LOAD_ACTIVE_LEVEL) {
        reset_measurement(true);
    } else if (g_state == BRD_STATE_SPINNING_LOADED) {
        /* [V0.10 新增] 排除先卸載、後收到 RPM 才被誤認為一次發射的情況。 */
        if ((uint32_t)(millis() - g_first_edge_ms) < 0x7FFFFFFFUL / 1000UL &&
            (int32_t)(edge_us - g_first_edge_us) < 0) {
            reset_measurement(false);
            return;
        }
        g_launch_ms = millis() - (uint32_t)(micros() - edge_us) / 1000UL;
        g_state = BRD_STATE_SPINNING_LAUNCHED;
        check_finish_threshold();
    } else if (g_state == BRD_STATE_LOADED_READY) {
        reset_measurement(false);
    }
}

static void update_load_event(void) {
    uint32_t sequence;
    uint32_t edge_us;
    int edge_level;
    int actual_level;

    portENTER_CRITICAL(&g_input_mux);
    sequence = g_load_sequence;
    edge_us = g_load_edge_us;
    edge_level = g_load_level;
    actual_level = (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
    portEXIT_CRITICAL(&g_input_mux);

    if (sequence != g_load_seen_sequence) {
        g_load_seen_sequence = sequence;
        g_load_raw = edge_level;
        /* [V0.10 修改] 即使最後電位相同，也必須以最新邊沿重新計算去抖。 */
        g_load_candidate_us = edge_us;
    }

    uint32_t now_us = micros();
    if (actual_level != g_load_raw) {
        /* [V0.10 新增] 補捉初始化邊界或 ISR 尚未送達的電位改變。 */
        g_load_raw = actual_level;
        g_load_candidate_us = now_us;
    }

    if (g_load_raw == g_load_stable ||
        (uint32_t)(now_us - g_load_candidate_us) < LOAD_IR_DEBOUNCE_US) {
        return;
    }

    portENTER_CRITICAL(&g_input_mux);
    bool unchanged = sequence == g_load_sequence &&
        g_load_raw == (int)((GPIO.in.val >> LOAD_IR_GPIO) & 1U);
    portEXIT_CRITICAL(&g_input_mux);

    if (unchanged) {
        handle_load_change(g_load_raw, g_load_candidate_us);
    }
}

static void update_timeouts(void) {
    if (g_state != BRD_STATE_SPINNING_LOADED &&
        g_state != BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }

    uint32_t quiet_us = (uint32_t)(micros() - g_last_activity_us);
    if (quiet_us >= RPM_ZERO_TIMEOUT_MS * 1000UL) {
        g_current_rpm = 0U;
        /* [V0.10 修改] 下次脈衝重新建立週期，不把停止時間當成一圈。 */
        g_period_valid = false;
        if (g_state == BRD_STATE_SPINNING_LAUNCHED && g_has_valid_rpm) {
            finish_measurement();
            return;
        }
    }

    if (g_state == BRD_STATE_SPINNING_LOADED &&
        quiet_us >= PRELAUNCH_IDLE_RESET_MS * 1000UL) {
        reset_measurement(true);
    } else if (g_state == BRD_STATE_SPINNING_LAUNCHED && !g_has_valid_rpm &&
        (uint32_t)(millis() - g_launch_ms) >= POST_LAUNCH_NO_RPM_TIMEOUT_MS) {
        finish_measurement();
    }
}

void brd_measurement_begin(void) {
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

    /* [V0.10 新增] 先禁止 ISR 接收新事件，再解除中斷掛載，避免停用途中留下事件。 */
    portENTER_CRITICAL(&g_input_mux);
    g_rpm_capture = false;
    g_load_capture = false;
    g_rpm_head = 0U;
    g_rpm_tail = 0U;
    g_rpm_overflow = false;
    g_load_sequence = 0UL;
    portEXIT_CRITICAL(&g_input_mux);

    if (g_measurement_interrupts_attached) {
        detachInterrupt(digitalPinToInterrupt(RPM_IR_GPIO));
        detachInterrupt(digitalPinToInterrupt(LOAD_IR_GPIO));
        g_measurement_interrupts_attached = false;
    }

    /* [V0.10 新增] 取消舊 MAX 與自鎖；低電警示的優先權高於量測結果。 */
    reset_measurement(false);
    g_load_raw = LOW;
    g_load_stable = LOW;
    g_load_seen_sequence = 0UL;
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

    /* [保留優先序] 先計算 RPM，再處理 LOAD，最後處理結束條件。 */
    update_rpm_events();
    if (g_max_lock) {
        return;
    }
    update_load_event();
    update_timeouts();
}

brd_display_t brd_measurement_get_display(void) {
    brd_display_t display;
    display.loaded = g_load_stable == LOAD_ACTIVE_LEVEL;
    display.show_max = g_show_max;
    display.value = g_show_max ? g_display_max : g_current_rpm;
    display.generation = g_display_generation;
    /* [V0.10 新增] HOLD 依既有自鎖旗標更新，不以 MAX 是否仍顯示來判定。 */
    display.hold_active = g_show_max && g_max_lock;
    return display;
}

void brd_measurement_max_frame_presented(uint32_t generation) {
    if (!g_measurement_enabled || !g_show_max ||
        generation != g_display_generation || g_max_frame_seen) {
        return;
    }

    /*
        [V0.10 修改] 以整幅 MAX 畫面成功送出時間開始計算 OLED_MAX_HOLD_MS。
        每筆結果只確認一次，週期性重繪不會反覆延長自鎖。
    */
    g_max_frame_seen = true;
    g_max_lock = true;
    g_max_lock_start_ms = millis();
    portENTER_CRITICAL(&g_input_mux);
    g_load_capture = false;
    portEXIT_CRITICAL(&g_input_mux);
}
