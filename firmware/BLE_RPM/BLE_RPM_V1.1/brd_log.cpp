/*
    檔案名稱: brd_log.cpp
    檔案位置: BLE_RPM_V1.0/brd_log.cpp

    啟動 LOG 與每秒執行狀態 LOG。
*/

#include <Arduino.h>

#include "brd_config.h"
#include "brd_context.h"
#include "brd_log.h"
#include "brd_measurement.h"
#include "brd_utils.h"

void brd_log_print_startup(void) {
#if (LOG_MASTER && LOG_SYSTEM)
    brd_context_t &ctx = brd_context_get();

    Serial.println();
    Serial.println(PROJECT_FULL_NAME);
    Serial.println("Firmware: BRD Dual IR Launch Recorder");
    Serial.print("BLE name: ");
    Serial.println(ctx.ble_device_name);
    Serial.println("GPIO0: RPM IR polling");
    Serial.println("GPIO1: HIGH=loaded, HIGH->LOW=launch");
    Serial.println("GPIO8: loaded RPM LED or charge LED");
    Serial.print("RPM sample interval: ");
    Serial.print(RPM_SAMPLE_INTERVAL_MS);
    Serial.println("ms");
    Serial.print("Stop RPM threshold: ");
    Serial.println(STOP_RPM_THRESHOLD);
#endif
}

void brd_log_print_ready(void) {
#if (LOG_MASTER && LOG_SYSTEM)
    Serial.println("BRD system ready");
#endif
}

void brd_log_update_task(void) {
#if (LOG_MASTER && LOG_RUNTIME)
    brd_context_t &ctx = brd_context_get();
    uint32_t now_ms;
    uint32_t elapsed_ms;
    uint32_t edge_per_sec;

    now_ms = millis();

    if ((uint32_t)(now_ms - ctx.log_last_ms) < LOG_INTERVAL_MS) {
        return;
    }

    elapsed_ms = now_ms - ctx.log_last_ms;

    if (elapsed_ms == 0UL) {
        return;
    }

    edge_per_sec = ((ctx.valid_edge_count - ctx.log_last_edge_count) * 1000UL) /
                   elapsed_ms;

    Serial.print("BRD runtime: state=");
    Serial.print(brd_state_to_text(ctx.brd_state));
    Serial.print(" loaded=");
    Serial.print(brd_load_is_active() ? 1 : 0);
    Serial.print(" charging=");
    Serial.print(ctx.charging ? 1 : 0);
    Serial.print(" rpm=");
    Serial.print(ctx.current_rpm);
    Serial.print(" max_rpm=");
    Serial.print(ctx.max_rpm);
    Serial.print(" launch_rpm=");
    Serial.print(ctx.launch_rpm);
    Serial.print(" edge_per_sec=");
    Serial.print(edge_per_sec);
    Serial.print(" samples=");
    Serial.println(ctx.curve_count);

    ctx.log_last_edge_count = ctx.valid_edge_count;
    ctx.log_last_ms = now_ms;
#endif
}
