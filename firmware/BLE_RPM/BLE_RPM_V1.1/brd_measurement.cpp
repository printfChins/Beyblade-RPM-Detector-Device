/*
    檔案名稱: brd_measurement.cpp
    檔案位置: BLE_RPM_V1.0/brd_measurement.cpp

    RPM 計算、GPIO 輪巡、發射判斷、
    量測狀態機、停止判斷與曲線記錄。
*/

#include <Arduino.h>

#include "brd_config.h"
#include "brd_context.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_utils.h"

static bool rpm_convert_from_period_us(uint32_t period_us, uint16_t *rpm_out);
static void curve_clear(void);
static bool curve_add_sample(uint16_t time_ms, uint16_t rpm);
static void measurement_data_reset(void);
static void measurement_start(uint32_t first_edge_us);
static void measurement_mark_launch(void);
static void measurement_finish(const char *reason);
static void load_handle_stable_change(int old_level, int new_level);
static bool rpm_ir_is_trigger_edge(int old_level, int new_level);
static void rpm_handle_trigger_edge(uint32_t now_us);

bool brd_load_is_active(void) {
    brd_context_t &ctx = brd_context_get();

    return (ctx.load_stable_level == LOAD_ACTIVE_LEVEL);
}

bool brd_measurement_is_active(void) {
    brd_context_t &ctx = brd_context_get();

    if (ctx.brd_state == BRD_STATE_SPINNING_LOADED) {
        return true;
    }

    if (ctx.brd_state == BRD_STATE_SPINNING_LAUNCHED) {
        return true;
    }

    return false;
}

uint16_t brd_measurement_elapsed_ms(void) {
    brd_context_t &ctx = brd_context_get();

    if (brd_measurement_is_active() == false) {
        if (ctx.brd_state == BRD_STATE_RESULT_PENDING) {
            if (ctx.curve_count == 0U) {
                return 0U;
            }

            return ctx.curve_buffer[ctx.curve_count - 1U].time_ms;
        }

        return 0U;
    }

    return brd_limit_u32_to_u16(millis() - ctx.record_start_ms);
}

void brd_measurement_begin(void) {
    brd_context_t &ctx = brd_context_get();
    uint32_t now_us;

    pinMode(RPM_IR_GPIO, RPM_IR_INPUT_MODE);
    pinMode(LOAD_IR_GPIO, LOAD_IR_INPUT_MODE);

    now_us = micros();

    ctx.rpm_raw_level = digitalRead(RPM_IR_GPIO);
    ctx.rpm_stable_level = ctx.rpm_raw_level;
    ctx.rpm_last_raw_change_us = now_us;
    ctx.rpm_last_poll_us = now_us;

    ctx.load_raw_level = digitalRead(LOAD_IR_GPIO);
    ctx.load_stable_level = ctx.load_raw_level;
    ctx.load_last_raw_change_us = now_us;
    ctx.load_last_poll_us = now_us;

    ctx.valid_edge_count = 0;
    ctx.log_last_ms = millis();
    ctx.log_last_edge_count = 0;

    if (brd_load_is_active() == true) {
        brd_enter_loaded_ready_state();
    } else {
        brd_enter_wait_load_state();
    }
}

static bool rpm_convert_from_period_us(uint32_t period_us, uint16_t *rpm_out) {
    uint32_t rpm;

    if (rpm_out == nullptr) {
#if (LOG_MASTER && LOG_RPM_REJECT)
        Serial.println("BRD rpm reject: rpm_out null");
#endif
        return false;
    }

    if ((period_us == 0UL) || (PULSES_PER_REV == 0UL)) {
#if (LOG_MASTER && LOG_RPM_REJECT)
        Serial.println("BRD rpm reject: invalid divisor");
#endif
        return false;
    }

    if (period_us < RPM_MIN_PERIOD_US) {
#if (LOG_MASTER && LOG_RPM_REJECT)
        Serial.print("BRD rpm reject: period too short, period_us=");
        Serial.println(period_us);
#endif
        return false;
    }

    if (period_us > RPM_MAX_PERIOD_US) {
#if (LOG_MASTER && LOG_RPM_REJECT)
        Serial.print("BRD rpm reject: period too long, period_us=");
        Serial.println(period_us);
#endif
        return false;
    }

    rpm = 60000000UL / period_us;
    rpm = rpm / PULSES_PER_REV;

    if (rpm > RPM_VALID_MAX) {
#if (LOG_MASTER && LOG_RPM_REJECT)
        Serial.print("BRD rpm reject: rpm over max, rpm=");
        Serial.println(rpm);
#endif
        return false;
    }

    *rpm_out = (uint16_t)rpm;

    return true;
}

static void curve_clear(void) {
    brd_context_t &ctx = brd_context_get();

    ctx.curve_count = 0;
}

static bool curve_add_sample(uint16_t time_ms, uint16_t rpm) {
    brd_context_t &ctx = brd_context_get();

    if (ctx.curve_count >= MAX_RECORD_SAMPLES) {
        return false;
    }

    ctx.curve_buffer[ctx.curve_count].time_ms = time_ms;
    ctx.curve_buffer[ctx.curve_count].rpm = rpm;
    ctx.curve_count++;

    return true;
}

static void measurement_data_reset(void) {
    brd_context_t &ctx = brd_context_get();

    curve_clear();

    ctx.last_trigger_us = 0;
    ctx.last_accepted_edge_ms = 0;

    ctx.current_rpm = 0;
    ctx.max_rpm = 0;
    ctx.has_valid_rpm = false;

    ctx.record_start_ms = 0;
    ctx.last_sample_ms = 0;

    ctx.launch_mark_valid = false;
    ctx.launch_rpm = 0;
    ctx.launch_time_ms = 0;
    ctx.launch_sample_index = INVALID_SAMPLE_INDEX;
    ctx.launch_absolute_ms = 0;

    ctx.stop_hold_active = false;
    ctx.stop_hold_start_ms = 0;

    ctx.launch_event_pending = false;
}

void brd_enter_wait_load_state(void) {
    brd_context_t &ctx = brd_context_get();

    measurement_data_reset();
    ctx.brd_state = BRD_STATE_WAIT_LOAD;

#if (LOG_MASTER && LOG_RECORD)
    Serial.println("BRD state: wait load");
#endif
}

void brd_enter_loaded_ready_state(void) {
    brd_context_t &ctx = brd_context_get();

    measurement_data_reset();
    ctx.brd_state = BRD_STATE_LOADED_READY;

#if (LOG_MASTER && LOG_RECORD)
    Serial.println("BRD state: loaded ready");
#endif
}

void brd_measurement_reset_after_result_send(void) {
    if (brd_load_is_active() == true) {
        brd_enter_loaded_ready_state();
    } else {
        brd_enter_wait_load_state();
    }
}

static void measurement_start(uint32_t first_edge_us) {
    brd_context_t &ctx = brd_context_get();

    if (ctx.brd_state != BRD_STATE_LOADED_READY) {
        return;
    }

    curve_clear();

    ctx.record_start_ms = millis();
    ctx.last_sample_ms = ctx.record_start_ms;

    ctx.last_trigger_us = first_edge_us;
    ctx.last_accepted_edge_ms = ctx.record_start_ms;
    ctx.valid_edge_count++;

    ctx.current_rpm = 0;
    ctx.max_rpm = 0;
    ctx.has_valid_rpm = false;

    ctx.launch_mark_valid = false;
    ctx.launch_rpm = 0;
    ctx.launch_time_ms = 0;
    ctx.launch_sample_index = INVALID_SAMPLE_INDEX;
    ctx.launch_absolute_ms = 0;

    ctx.stop_hold_active = false;
    ctx.stop_hold_start_ms = 0;

    curve_add_sample(0U, 0U);

    ctx.brd_state = BRD_STATE_SPINNING_LOADED;

    brd_status_led_on_rpm_trigger();

#if (LOG_MASTER && LOG_RECORD)
    Serial.println("BRD record start: loaded and first RPM edge detected");
#endif
}

static void measurement_mark_launch(void) {
    brd_context_t &ctx = brd_context_get();
    uint16_t elapsed_ms;
    bool sample_added;

    if (ctx.brd_state != BRD_STATE_SPINNING_LOADED) {
        return;
    }

    elapsed_ms = brd_limit_u32_to_u16(millis() - ctx.record_start_ms);

    ctx.launch_rpm = ctx.current_rpm;
    ctx.launch_time_ms = elapsed_ms;
    ctx.launch_absolute_ms = millis();

    if (ctx.current_rpm > ctx.max_rpm) {
        ctx.max_rpm = ctx.current_rpm;
    }

    if ((ctx.curve_count > 0U) &&
        (ctx.curve_buffer[ctx.curve_count - 1U].time_ms == elapsed_ms)) {
        ctx.curve_buffer[ctx.curve_count - 1U].rpm = ctx.current_rpm;
        ctx.launch_sample_index = ctx.curve_count - 1U;
        sample_added = true;
    } else {
        ctx.launch_sample_index = ctx.curve_count;
        sample_added = curve_add_sample(elapsed_ms, ctx.current_rpm);
    }

    if (sample_added == false) {
        if (ctx.curve_count > 0U) {
            ctx.launch_sample_index = ctx.curve_count - 1U;
            ctx.curve_buffer[ctx.launch_sample_index].time_ms = elapsed_ms;
            ctx.curve_buffer[ctx.launch_sample_index].rpm = ctx.current_rpm;
        } else {
            ctx.launch_sample_index = INVALID_SAMPLE_INDEX;
        }
    }

    ctx.launch_mark_valid = (ctx.launch_sample_index != INVALID_SAMPLE_INDEX);
    ctx.launch_event_pending = true;

    ctx.stop_hold_active = false;
    ctx.stop_hold_start_ms = 0;

    ctx.brd_state = BRD_STATE_SPINNING_LAUNCHED;

#if (LOG_MASTER && LOG_RECORD)
    Serial.print("BRD launch: rpm=");
    Serial.print(ctx.launch_rpm);
    Serial.print(" max_rpm=");
    Serial.print(ctx.max_rpm);
    Serial.print(" time_ms=");
    Serial.print(ctx.launch_time_ms);
    Serial.print(" sample_index=");
    Serial.println(ctx.launch_sample_index);
#endif
}

static void measurement_finish(const char *reason) {
    brd_context_t &ctx = brd_context_get();
    uint16_t elapsed_ms;

    if (ctx.brd_state != BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }

    elapsed_ms = brd_limit_u32_to_u16(millis() - ctx.record_start_ms);

    if ((ctx.curve_count == 0U) ||
        (ctx.curve_buffer[ctx.curve_count - 1U].time_ms != elapsed_ms) ||
        (ctx.curve_buffer[ctx.curve_count - 1U].rpm != ctx.current_rpm)) {
        curve_add_sample(elapsed_ms, ctx.current_rpm);
    }

    if (ctx.curve_count >= MIN_RECORD_SAMPLES) {
        ctx.brd_state = BRD_STATE_RESULT_PENDING;

#if (LOG_MASTER && LOG_RECORD)
        Serial.print("BRD record finish: reason=");
        Serial.print(reason);
        Serial.print(" samples=");
        Serial.print(ctx.curve_count);
        Serial.print(" max_rpm=");
        Serial.print(ctx.max_rpm);
        Serial.print(" launch_rpm=");
        Serial.println(ctx.launch_rpm);
#endif
    } else {
#if (LOG_MASTER && LOG_RECORD)
        Serial.println("BRD record discard: too few samples");
#endif

        if (brd_load_is_active() == true) {
            brd_enter_loaded_ready_state();
        } else {
            brd_enter_wait_load_state();
        }
    }
}

static void load_handle_stable_change(int old_level, int new_level) {
    brd_context_t &ctx = brd_context_get();
    bool old_loaded;
    bool new_loaded;

    old_loaded = (old_level == LOAD_ACTIVE_LEVEL);
    new_loaded = (new_level == LOAD_ACTIVE_LEVEL);

#if (LOG_MASTER && LOG_LOAD)
    Serial.print("BRD load change: old=");
    Serial.print(brd_log_level_to_text(old_level));
    Serial.print(" new=");
    Serial.print(brd_log_level_to_text(new_level));
    Serial.print(" loaded=");
    Serial.println(new_loaded ? 1 : 0);
#endif

    if ((old_loaded == false) && (new_loaded == true)) {
        if ((ctx.brd_state == BRD_STATE_WAIT_LOAD) ||
            (ctx.brd_state == BRD_STATE_LOADED_READY)) {
            brd_enter_loaded_ready_state();
        }

        return;
    }

    if ((old_loaded == true) && (new_loaded == false)) {
        if (ctx.brd_state == BRD_STATE_SPINNING_LOADED) {
            measurement_mark_launch();
            return;
        }

        if (ctx.brd_state == BRD_STATE_LOADED_READY) {
            brd_enter_wait_load_state();
            return;
        }

        /*
            已進入 SPINNING_LAUNCHED 或 RESULT_PENDING 時，
            不因 GPIO1 LOW 清除量測資料。
        */
    }
}

void brd_load_poll_update(void) {
    brd_context_t &ctx = brd_context_get();
    uint32_t now_us;
    int level;

    now_us = micros();

    if ((uint32_t)(now_us - ctx.load_last_poll_us) < LOAD_IR_POLL_INTERVAL_US) {
        return;
    }

    ctx.load_last_poll_us = now_us;
    level = digitalRead(LOAD_IR_GPIO);

    if (level != ctx.load_raw_level) {
        ctx.load_raw_level = level;
        ctx.load_last_raw_change_us = now_us;
        return;
    }

    if ((level != ctx.load_stable_level) &&
        ((uint32_t)(now_us - ctx.load_last_raw_change_us) >= LOAD_IR_STABLE_TIME_US)) {
        int old_stable_level;

        old_stable_level = ctx.load_stable_level;
        ctx.load_stable_level = level;

        load_handle_stable_change(old_stable_level, ctx.load_stable_level);
    }
}

static bool rpm_ir_is_trigger_edge(int old_level, int new_level) {
    if ((RPM_IR_TRIGGER_EDGE == FALLING) &&
        (old_level == HIGH) &&
        (new_level == LOW)) {
        return true;
    }

    if ((RPM_IR_TRIGGER_EDGE == RISING) &&
        (old_level == LOW) &&
        (new_level == HIGH)) {
        return true;
    }

    return false;
}

static void rpm_handle_trigger_edge(uint32_t now_us) {
    brd_context_t &ctx = brd_context_get();
    uint32_t period_us;
    uint16_t rpm;

    if ((ctx.brd_state == BRD_STATE_WAIT_LOAD) ||
        (ctx.brd_state == BRD_STATE_RESULT_PENDING)) {
        return;
    }

    if (ctx.brd_state == BRD_STATE_LOADED_READY) {
        if (brd_load_is_active() == true) {
            measurement_start(now_us);
        }

        return;
    }

    if (brd_measurement_is_active() == false) {
        return;
    }

    if (ctx.last_trigger_us == 0UL) {
        ctx.last_trigger_us = now_us;
        ctx.last_accepted_edge_ms = millis();
        ctx.valid_edge_count++;
        brd_status_led_on_rpm_trigger();
        return;
    }

    period_us = now_us - ctx.last_trigger_us;

    if (period_us < RPM_MIN_PERIOD_US) {
#if (LOG_MASTER && LOG_RPM_REJECT)
        Serial.print("BRD trigger reject: period too short, period_us=");
        Serial.println(period_us);
#endif
        return;
    }

    if (period_us > RPM_MAX_PERIOD_US) {
        ctx.last_trigger_us = now_us;
        ctx.last_accepted_edge_ms = millis();
        ctx.current_rpm = 0;
        ctx.valid_edge_count++;
        brd_status_led_on_rpm_trigger();

#if (LOG_MASTER && LOG_RPM_REJECT)
        Serial.print("BRD trigger restart: period too long, period_us=");
        Serial.println(period_us);
#endif
        return;
    }

    if (rpm_convert_from_period_us(period_us, &rpm) == false) {
        return;
    }

    ctx.current_rpm = rpm;
    ctx.last_trigger_us = now_us;
    ctx.last_accepted_edge_ms = millis();
    ctx.valid_edge_count++;
    ctx.has_valid_rpm = true;

    if (ctx.current_rpm > ctx.max_rpm) {
        ctx.max_rpm = ctx.current_rpm;
    }

    brd_status_led_on_rpm_trigger();

#if (LOG_MASTER && LOG_EDGE_TRIGGER)
    Serial.print("BRD rpm edge: period_us=");
    Serial.print(period_us);
    Serial.print(" rpm=");
    Serial.print(ctx.current_rpm);
    Serial.print(" max_rpm=");
    Serial.println(ctx.max_rpm);
#endif
}

void brd_rpm_poll_update(void) {
    brd_context_t &ctx = brd_context_get();
    uint32_t now_us;
    int level;

    now_us = micros();

    if ((uint32_t)(now_us - ctx.rpm_last_poll_us) < RPM_IR_POLL_INTERVAL_US) {
        return;
    }

    ctx.rpm_last_poll_us = now_us;
    level = digitalRead(RPM_IR_GPIO);

    if (level != ctx.rpm_raw_level) {
        ctx.rpm_raw_level = level;
        ctx.rpm_last_raw_change_us = now_us;
        return;
    }

    if ((level != ctx.rpm_stable_level) &&
        ((uint32_t)(now_us - ctx.rpm_last_raw_change_us) >= RPM_IR_STABLE_TIME_US)) {
        int old_stable_level;
        bool trigger;

        old_stable_level = ctx.rpm_stable_level;
        ctx.rpm_stable_level = level;

        trigger = rpm_ir_is_trigger_edge(old_stable_level, ctx.rpm_stable_level);

        if (trigger == true) {
            rpm_handle_trigger_edge(now_us);
        }
    }
}

void brd_rpm_zero_update_task(void) {
    brd_context_t &ctx = brd_context_get();
    uint32_t now_ms;

    if (brd_measurement_is_active() == false) {
        return;
    }

    if (ctx.last_accepted_edge_ms == 0UL) {
        return;
    }

    now_ms = millis();

    if ((uint32_t)(now_ms - ctx.last_accepted_edge_ms) >= RPM_ZERO_TIMEOUT_MS) {
        ctx.current_rpm = 0;
    }
}

void brd_measurement_stop_update_task(void) {
    brd_context_t &ctx = brd_context_get();
    uint32_t now_ms;

    now_ms = millis();

    if (ctx.brd_state == BRD_STATE_SPINNING_LOADED) {
        if ((ctx.last_accepted_edge_ms > 0UL) &&
            ((uint32_t)(now_ms - ctx.last_accepted_edge_ms) >= PRELAUNCH_IDLE_RESET_MS)) {
#if (LOG_MASTER && LOG_RECORD)
            Serial.println("BRD prelaunch record reset: RPM idle timeout");
#endif

            if (brd_load_is_active() == true) {
                brd_enter_loaded_ready_state();
            } else {
                brd_enter_wait_load_state();
            }
        }

        return;
    }

    if (ctx.brd_state != BRD_STATE_SPINNING_LAUNCHED) {
        return;
    }

    if (ctx.has_valid_rpm == false) {
        if ((ctx.launch_absolute_ms > 0UL) &&
            ((uint32_t)(now_ms - ctx.launch_absolute_ms) >= POST_LAUNCH_NO_RPM_TIMEOUT_MS)) {
            ctx.current_rpm = 0;
            measurement_finish("post-launch no valid RPM");
        }

        return;
    }

    if (ctx.current_rpm <= STOP_RPM_THRESHOLD) {
        if (ctx.stop_hold_active == false) {
            ctx.stop_hold_active = true;
            ctx.stop_hold_start_ms = now_ms;
            return;
        }

        if ((uint32_t)(now_ms - ctx.stop_hold_start_ms) >= STOP_RPM_HOLD_MS) {
            measurement_finish("RPM reached stop threshold");
        }

        return;
    }

    ctx.stop_hold_active = false;
    ctx.stop_hold_start_ms = 0;
}

void brd_curve_record_update_task(void) {
    brd_context_t &ctx = brd_context_get();
    uint32_t now_ms;
    uint16_t elapsed_ms;

    if (brd_measurement_is_active() == false) {
        return;
    }

    now_ms = millis();

    while ((uint32_t)(now_ms - ctx.last_sample_ms) >= RPM_SAMPLE_INTERVAL_MS) {
        ctx.last_sample_ms += RPM_SAMPLE_INTERVAL_MS;
        elapsed_ms = brd_limit_u32_to_u16(ctx.last_sample_ms - ctx.record_start_ms);

        if (curve_add_sample(elapsed_ms, ctx.current_rpm) == false) {
            if (ctx.brd_state == BRD_STATE_SPINNING_LAUNCHED) {
                measurement_finish("curve buffer full");
            }

            return;
        }
    }
}
