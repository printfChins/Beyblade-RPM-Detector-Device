/*
    檔案名稱: brd_context.cpp
    檔案位置: BLE_RPM_V1.0/brd_context.cpp

    實作 BRD 共用狀態物件與初始值設定。
*/

#include "brd_context.h"

#include <cstring>

static brd_context_t g_brd_context;

brd_context_t &brd_context_get(void) {
    return g_brd_context;
}

void brd_context_reset(void) {
    memset(&g_brd_context, 0, sizeof(g_brd_context));

    snprintf(g_brd_context.ble_device_name,
             sizeof(g_brd_context.ble_device_name),
             "%s_0000",
             PROJECT_SHORT_NAME);

    g_brd_context.rpm_raw_level = HIGH;
    g_brd_context.rpm_stable_level = HIGH;
    g_brd_context.load_raw_level = HIGH;
    g_brd_context.load_stable_level = HIGH;

    g_brd_context.brd_state = BRD_STATE_WAIT_LOAD;
    g_brd_context.launch_sample_index = INVALID_SAMPLE_INDEX;
    g_brd_context.led_trigger_output_on = true;
}
