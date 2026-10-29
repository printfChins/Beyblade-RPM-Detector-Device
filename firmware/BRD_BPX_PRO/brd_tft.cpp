/* 檔案: BRD_BPX_PRO/brd_tft.cpp
 * [重寫] 此檔僅負責從各模組收集 UI 狀態及節流。
 * [刪除] 全畫面繪圖、Adafruit 直接呼叫、SPI 和字庫處理。
 * [保留] brd_tft_begin()/brd_tft_update() 對上層的相容 API。
 */
#include <Arduino.h>
#include "brd_config.h"
#include "brd_battery.h"
#include "brd_bbp.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_settings_ble.h"
#include "brd_io.h"
#include "brd_load_adc.h"
#include "brd_rpm_adc.h"
#include "brd_ota.h"
#include "brd_tft.h"
#include "brd_ui_core.h"
#include "brd_ui_input.h"
#include "brd_ui_curve.h"
namespace {
uint32_t last_update_ms = 0;
uint32_t last_retry_ms = 0;
// [新增] IR TEST 專用 ADC 取樣；GPIO1 / GPIO4 讀值直接提供動畫畫面。
// [保留] 主畫面、曲線、歷史頁不增加 ADC 取樣工作。
static void sample_ir_adc(brd_ui::Model &model) {
    if (model.low || model.adc_fault || model.ota.active) {
        return;
    }
    brd_load_adc_update();
    brd_rpm_adc_update();
    model.load_adc_valid = brd_load_adc_valid();
    model.load_adc_raw = brd_load_adc_raw();
    model.rpm_adc_valid = brd_rpm_adc_valid();
    model.rpm_adc_raw = brd_rpm_adc_raw();
}
static brd_ui::Model snapshot() {
    brd_ui::Model model;
    model.low = brd_battery_is_low_locked();
    model.adc_fault = !model.low && brd_battery_is_adc_fault();
    model.ota = brd_ota_get_display();
    model.measurement = brd_measurement_get_display();
    model.requested_page = brd_settings_ble_display_page();
    model.battery = brd_battery_get_percent();
    model.voltage_mv = brd_battery_get_voltage_mv();
    model.charging = brd_io_is_charging();
    // [新增] 相位與真實電量無關；只在充電時啟用，不改變 ADC 量測結果。
    model.charge_phase = model.charging ?
        static_cast<uint8_t>((millis() / BRD_UI_CHARGE_ANIM_INTERVAL_MS) % 4UL) : 0U;
    model.connected = brd_bbp_is_connected();
    model.ble_disabled = model.charging &&
        brd_settings_get().charging_ble_off != 0U && !brd_bbp_is_started();
    model.ir1 = brd_io_read_ir1();
    model.ir2 = brd_io_read_ir2();
    // [修改] BLE 或 GPIO5 第四頁要求 IR TEST 時均更新 ADC。
    if (model.requested_page == BRD_DISPLAY_PAGE_IR_TEST ||
        brd_ui::input_current_page() == 3U) {
        sample_ir_adc(model);
    }
    model.history_count = brd_bbp_recent_history(model.history, &model.history_peak);
    model.mode_auto = brd_settings_get().mode == BRD_MEASUREMENT_MODE_AUTO;
    return model;
}
}
void brd_tft_begin(void) {
    brd_ui::input_begin();
    brd_rpm_adc_begin();
    brd_ui::curve_begin();
    if (!brd_ui::begin()) { return; }
    brd_ui::Model initial = snapshot();
    if (!initial.low && !initial.adc_fault) {
        brd_ui::show_boot();
        brd_ui::animate_boot(TFT_BOOT_VERSION_DISPLAY_MS); // [修改] BOOT 8 格由 0 漸亮至滿格。
    } else {
        brd_ui::render(initial);
    }
    last_update_ms = millis() - TFT_UPDATE_INTERVAL_MS;
}
void brd_tft_update(void) {
    if (!brd_ui::ready()) {
        if ((uint32_t)(millis() - last_retry_ms) < 1000UL) { return; }
        last_retry_ms = millis();
        if (!brd_ui::begin()) { return; }
    }
    /* [修改] 一般頁以 CFG 排程取得動畫更新機會；資料不變時由 Dirty Tile 抑制 SPI。
     * [保留] 警告/OTA/轉速測量的優先權，主程式及中斷無需修改。
     */
    static bool last_warning = false, last_ota = false, last_fault = false;
    static uint8_t last_page = 0xffU;
    brd_ui::Model model = snapshot();
    // [修改] 警告、OTA、BLE 指定 IR TEST 時不允許本地四頁切換。
    const bool nav_allowed = !model.low && !model.adc_fault && !model.ota.active &&
        model.requested_page == BRD_DISPLAY_PAGE_MAIN;
    const bool toggled = brd_ui::input_update(nav_allowed);
    model.graph_selected = brd_ui::input_graph_selected();
    model.local_page = brd_ui::input_current_page();
    // [新增] 同一次短按剛進入 IR TEST 時立即取得 ADC 快照，避免第一幀空白。
    if (toggled && model.local_page == 3U) {
        sample_ir_adc(model);
    }
    // [修改] 曲線由量測主迴圈事件寫入，不再依 TFT 刷新週期取樣。
    const bool urgent = toggled || model.low != last_warning || model.adc_fault != last_fault ||
        model.ota.active != last_ota || model.requested_page != last_page;
    /* [修改] 外層輪詢依 BRD_TFT_POLL_INTERVAL_MS（目前 10 ms）更新。
     * [保留] 警告頁每 100 ms 檢查一次，內層仍每 500 ms 更新一幀。
     */
    const uint32_t interval = (model.low || model.adc_fault) ?
        BRD_TFT_WARNING_POLL_INTERVAL_MS : BRD_TFT_POLL_INTERVAL_MS;
    if (!urgent && (uint32_t)(millis() - last_update_ms) < interval) { return; }
    (void)brd_ui::render(model);
    last_update_ms = millis();
    last_warning = model.low;
    last_fault = model.adc_fault;
    last_ota = model.ota.active;
    last_page = model.requested_page;
    /* [修改] 最高轉速在 MAIN 顯示；保留原有 MAX generation ACK 機制。 */
    if (!model.low && !model.adc_fault && !model.ota.active &&
        (brd_ui::current_page() == brd_ui::Page::Main ||
         brd_ui::current_page() == brd_ui::Page::Graph ||
         brd_ui::current_page() == brd_ui::Page::History) && model.measurement.show_max) {
        brd_measurement_max_frame_presented(model.measurement.generation);
    }
}
