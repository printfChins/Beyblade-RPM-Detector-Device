/*
    檔案名稱: brd_io.cpp
    檔案位置: BLE_RPM_V1.0/brd_io.cpp
        
    GPIO8 LED 與 GPIO10 充電偵測邏輯。
*/

#include <Arduino.h>

#include "brd_config.h"
#include "brd_context.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_utils.h"

static void status_led_write(bool on) {
    if (on == true) {
        digitalWrite(STATUS_LED_GPIO, STATUS_LED_ACTIVE_LEVEL);
    } else {
        digitalWrite(STATUS_LED_GPIO, STATUS_LED_INACTIVE_LEVEL);
    }
}

static void status_led_init(void) {
    brd_context_t &ctx = brd_context_get();

    digitalWrite(STATUS_LED_GPIO, STATUS_LED_INACTIVE_LEVEL);
    pinMode(STATUS_LED_GPIO, OUTPUT);

    ctx.led_trigger_output_on = true;
    ctx.led_last_trigger_ms = 0;
}

static void charge_gpio_init(void) {
    brd_context_t &ctx = brd_context_get();

    pinMode(CHRG_DET_GPIO, CHRG_DET_INPUT_MODE);

    ctx.charge_state_initialized = false;
    ctx.charging = false;
}

void brd_io_begin(void) {
    status_led_init();
    charge_gpio_init();
    brd_charge_update_task();
}

void brd_status_led_on_rpm_trigger(void) {
    brd_context_t &ctx = brd_context_get();

    if (brd_load_is_active() == false) {
        return;
    }

    ctx.led_trigger_output_on = !ctx.led_trigger_output_on;
    ctx.led_last_trigger_ms = millis();

#if (LOG_MASTER && LOG_LED)
    Serial.print("BRD led rpm trigger: on=");
    Serial.println(ctx.led_trigger_output_on ? 1 : 0);
#endif
}

void brd_status_led_update_task(void) {
    brd_context_t &ctx = brd_context_get();
    uint32_t now_ms;

    now_ms = millis();

    if (brd_load_is_active() == true) {
        if ((ctx.led_last_trigger_ms == 0UL) ||
            ((uint32_t)(now_ms - ctx.led_last_trigger_ms) >= LED_TRIGGER_RESTORE_MS)) {
            ctx.led_trigger_output_on = true;
        }

        status_led_write(ctx.led_trigger_output_on);
        return;
    }

    status_led_write(ctx.charging);
}

void brd_charge_update_task(void) {
    brd_context_t &ctx = brd_context_get();
    int charge_level;
    bool is_charging;

    charge_level = digitalRead(CHRG_DET_GPIO);
    is_charging = (charge_level == CHRG_ACTIVE_LEVEL);

    if ((ctx.charge_state_initialized == false) ||
        (is_charging != ctx.charging)) {
        ctx.charging = is_charging;
        ctx.charge_state_initialized = true;

#if (LOG_MASTER && LOG_CHARGE)
        Serial.print("BRD charge state: chrg_det=");
        Serial.print(brd_log_level_to_text(charge_level));
        Serial.print(" charging=");
        Serial.println(ctx.charging ? 1 : 0);
#endif
    }
}
