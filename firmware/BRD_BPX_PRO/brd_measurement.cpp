/*
    檔案位置: BRD_BPX_PRO/brd_measurement.cpp
    [新增] GPIO1 LOAD 轉 ADC 定期採樣，產生邏輯狀態事件並沿用 100ms 去抖狀態機。
    [刪減] GPIO1 CHANGE ISR、LOAD IRQ 佇列及 GPIO 數位取值。
    [修改] GPIO3 RPM CHANGE ISR 同時處理雙極性完整一圈，分極性 32 點 BLE 曲線。
    [保留] 事件時間排序、MAX、AUTO 模式與 BLE/OTA 封包。
    [保留] 已旋轉測速後的 LOAD 變化不得中止既有 RPM/MAX/曲線。
*/
#include <soc/gpio_struct.h>

#include "brd_config.h"
#include "brd_bbp.h"
#include "brd_measurement.h"
/* [新增] 四項參數讀取執行時設定，ISR 不存取設定模型。 */
#include "brd_settings.h"
#include "brd_load_adc.h"
#include "brd_ui_curve.h"
#include "brd_shot_capture.h" /* [新增 V1.4] RPM 事件先寫入固定緩衝，完成後才建立曲線 */

struct input_edge_t {
    uint32_t time_us;
    int level;
};

static portMUX_TYPE g_input_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile input_edge_t g_rpm_queue[RPM_ISR_QUEUE_SIZE];
static volatile uint16_t g_rpm_head = 0U;
static volatile uint16_t g_rpm_tail = 0U;
static volatile bool g_rpm_overflow = false;
static volatile bool g_input_capture = false;
/* [刪除] 單一參考極性鎖定；兩種極性皆參與獨立單圈測速。 */

/* [V0.12 新增] 固定 RAM 快照，不使用 heap，也不把陣列放在主迴圈堆疊。 */
static input_edge_t g_rpm_batch[RPM_ISR_QUEUE_SIZE];
/* [新增] ADC 取樣後生成的單一邏輯事件，與 RPM ISR event 依時間排序。 */
static input_edge_t g_load_batch[1];
static brd_measurement_diagnostics_t g_diagnostics = {};
static bool g_measurement_enabled = false;
/* [新增] BLE callback 只讀 critical-section 保護的修改窗口，不讀量測狀態機。 */
static bool g_modification_window = true;
static bool g_settings_paused = false;
static bool g_measurement_stopped = false;
static bool g_measurement_interrupts_attached = false;
static brd_state_t g_state = BRD_STATE_WAIT_LOAD;
static int g_load_raw = LOW;
static int g_load_stable = LOW;
static uint32_t g_load_candidate_us = 0UL;
/* [修改] HIGH/LOW 各自保留同極性週期基準，兩組並行。 */
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
    uint32_t edge_us = micros();
    int level = (int)((GPIO.in.val >> RPM_IR_GPIO) & 1U);
    portENTER_CRITICAL_ISR(&g_input_mux);
    if (g_input_capture) {
        /* [修改] 正緣及負緣均存入固定佇列；ISR 不執行浮點、除法或 BLE。 */
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

/* [刪減] LOAD 原 GPIO ISR / IRQ queue：GPIO1 改 ADC polling。 */

static void disable_input_capture(void) {
    portENTER_CRITICAL(&g_input_mux);
    g_input_capture = false;
    g_rpm_head = 0U;
    g_rpm_tail = 0U;
    g_rpm_overflow = false;
    portEXIT_CRITICAL(&g_input_mux);
}

static void restart_input_capture(void) {
    /* [修改] RPM queue 清零，LOAD 由 ADC 取樣後生成狀態事件。 */
    portENTER_CRITICAL(&g_input_mux);
    g_rpm_head = 0U;
    g_rpm_tail = 0U;
    g_rpm_overflow = false;
    g_load_raw = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    if (!measurement_uses_load()) { g_load_stable = LOAD_ACTIVE_LEVEL; }
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
    brd_shot::reset(); /* [新增] 原始單圈資料從新一發開始紀錄。 */
    brd_ui::curve_capture_reset(); /* [保留] 新一發才清除上一筆曲線。 */
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

    /* [BRD_BBP 新增] 凍結完整曲線，發布及 Flash 留給主 loop。 */
    /* [修改 V1.4] 只標記一發完成：TFT/BLE 曲線在主迴圈閒置路徑後處理。 */
    brd_shot::finish(g_max_rpm);
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

    // [修改] 兩組各自使用本極性上一次邊緣，沒有半圈 RPM 倍率換算。

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

    /* [修改] 正->正、負->負各有獨立 32 點 BLE 緩衝。由發射完成時的 MAX 選一組。 */
    brd_shot::record(period_us, edge_us, polarity, g_current_rpm); /* [修改] 每筆有效單圈記錄 */
    if (g_current_rpm > g_max_rpm) {
        g_max_rpm = g_current_rpm;
    }

    if (!measurement_uses_load() && !g_auto_launch_candidate &&
        g_current_rpm >= brd_settings_get().auto_min_rpm) {
        g_auto_launch_candidate = true;
    }

    check_launch_threshold(edge_us);
}

// [修改] confirmed_us 是 LOAD ADC 分類後維持 100ms 的確認時間。
static void handle_load_change(uint32_t confirmed_us) {
    (void)confirmed_us;
    if (!measurement_uses_load()) {
        return;
    }

    g_load_stable = g_load_raw;
    increment_counter(g_diagnostics.load_stable_transitions);

    /*
        [修正] RPM 已開始後，LOAD 只更新穩定顯示狀態，不得再重置量測。
        LOAD ACTIVE / INACTIVE 都不參與發射成功，也不能清除 RPM / MAX / 曲線。
    */
    if (g_state == BRD_STATE_SPINNING_LOADED ||
        g_state == BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }

    if (g_load_stable == LOAD_ACTIVE_LEVEL) {
        /*
            [修正] 只有 WAIT_LOAD -> LOAD ACTIVE 穩定 cfg 去抖時間 才能啟動新一輪量測。
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
                            bool &rpm_overflow) {
    rpm_count = 0U;
    load_count = 0U;
    /* [新增] ADC 不可在 critical section 中讀，且由 shared ADC1 owner 管理。 */
    if (measurement_uses_load()) { brd_load_adc_update(); }
    portENTER_CRITICAL(&g_input_mux);
    rpm_overflow = g_rpm_overflow;
    g_rpm_overflow = false;
    while (g_rpm_tail != g_rpm_head && rpm_count < RPM_ISR_QUEUE_SIZE) {
        g_rpm_batch[rpm_count].time_us = g_rpm_queue[g_rpm_tail].time_us;
        g_rpm_batch[rpm_count].level = g_rpm_queue[g_rpm_tail].level;
        rpm_count++;
        g_rpm_tail = (uint16_t)((g_rpm_tail + 1U) % RPM_ISR_QUEUE_SIZE);
    }
    now_us = micros();
    if (measurement_uses_load()) {
        int adc_level = brd_load_adc_active() ? LOAD_ACTIVE_LEVEL : LOW;
        if (adc_level != g_load_raw) {
            g_load_batch[0].time_us = now_us;
            g_load_batch[0].level = adc_level;
            load_count = 1U;
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
    brd_load_adc_begin(); /* [新增] ADC 分類器歸零，LOAD 重新等待穩定。 */
    reset_measurement(!measurement_uses_load());
    g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    restart_input_capture();
    attachInterrupt(digitalPinToInterrupt(RPM_IR_GPIO), rpm_ir_isr, RPM_IR_TRIGGER_EDGE);
    g_measurement_interrupts_attached = true;
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
    reset_measurement(!measurement_uses_load());
    g_load_raw = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    g_load_stable = measurement_uses_load() ? LOW : LOAD_ACTIVE_LEVEL;
    /* [BRD_BBP 新增] 低電/ADC 故障中斷的一發不發布。 */
    brd_shot::abort(); /* [新增] 低電、ADC 故障不發布半成品。 */
    brd_bbp_capture_abort();
}

void brd_measurement_update(void) {
    if (!g_measurement_enabled) {
        return;
    }
    if (g_max_lock) {
        if ((uint32_t)(millis() - g_max_lock_start_ms) < TFT_MAX_HOLD_MS) {
            return;
        }
        g_max_lock = false;
        /* [修改] HOLD 結束仍同時啟用正、負緣，無需鎖定單一極性。 */
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
        update_modification_window();
        restart_input_capture();
        return;
    }

    uint16_t rpm_count;
    uint16_t load_count;
    uint32_t now_us;
    bool rpm_overflow;
    snapshot_inputs(rpm_count, load_count, now_us, rpm_overflow);
    if (rpm_overflow) {
        /*
            [修正] RPM Queue overflow 不再 abort 整次 Capture。
            snapshot 內已保存的事件仍依序處理，保留最早 32 點、已取得的 MAX 與本次參考極性。
            缺失區段在批次處理完成後只重新建立同極性週期基準，避免把 Queue overflow
            誤判成 300 ms 無脈衝而提前完成發射。
        */
        increment_counter(g_diagnostics.rpm_queue_overflows);
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
            兩個極性週期基準都重新同步，避免跳緣造成錯誤峰值。
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

uint16_t brd_measurement_rpm_backlog(void) {
    portENTER_CRITICAL(&g_input_mux);
    const uint16_t head = g_rpm_head;
    const uint16_t tail = g_rpm_tail;
    portEXIT_CRITICAL(&g_input_mux);
    return head >= tail ? static_cast<uint16_t>(head - tail) :
           static_cast<uint16_t>(RPM_ISR_QUEUE_SIZE - tail + head);
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
    /* [新增] UI 專用峰值：LIVE 單調遞增，完成後與真正 MAX 完全一致。 */
    display.peak_value = g_show_max ? g_display_max : g_max_rpm;
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

/* [修改] LOAD 維持 WAIT_LOAD；AUTO 的 READY 亦屬非量測安全時段。 */
bool brd_measurement_storage_allowed(void) {
    return !g_measurement_enabled || g_state == BRD_STATE_WAIT_LOAD ||
           (!measurement_uses_load() && g_state == BRD_STATE_LOADED_READY);
}

/* [新增] callback 與主 loop 共用安全窗口；已排入 GPIO 事件時先處理量測。 */
bool brd_measurement_modifications_allowed(void) {
    portENTER_CRITICAL(&g_input_mux);
    bool allowed = g_modification_window && !g_rpm_overflow &&
                   g_rpm_head == g_rpm_tail && !g_settings_paused;
    portEXIT_CRITICAL(&g_input_mux);
    return allowed;
}
bool brd_measurement_pause_for_settings(void) {
    portENTER_CRITICAL(&g_input_mux);
    bool allowed = g_modification_window && !g_rpm_overflow &&
                   g_rpm_head == g_rpm_tail && !g_settings_paused;
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
        /* [修改] 模式切換不再掛載 LOAD IRQ，改為 ADC 分類器重新初始化。 */
        brd_load_adc_begin();
        g_period_valid[LOW] = g_period_valid[HIGH] = false;
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
