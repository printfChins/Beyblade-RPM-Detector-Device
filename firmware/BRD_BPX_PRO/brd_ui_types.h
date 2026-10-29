/* 檔案: BRD_BPX_PRO/brd_ui_types.h
 * [新增] UI 的唯讀資料快照及頁面 ID。新增頁面只需擴充 Page 與路由。
 */
#ifndef BRD_UI_TYPES_H
#define BRD_UI_TYPES_H
#include <stdint.h>
#include "brd_measurement.h"
#include "brd_ota.h"
namespace brd_ui {
enum class Page : uint8_t { Boot, Main, Max, Graph, History, IrTest, Ota, LowBattery, AdcFault };
struct Model {
    brd_display_t measurement = {};
    brd_ota_display_t ota = {};
    uint8_t battery = 0;
    uint16_t voltage_mv = 0;
    bool charging = false;
    // [新增] 充電動畫相位 (0..3)，由主迴圈每 240 ms 更新，不使用中斷或 delay。
    uint8_t charge_phase = 0U;
    bool connected = false;
    bool ble_disabled = false;
    bool ir1 = false;
    bool ir2 = false;
    /* [正式新增] ADC1_CH1 GPIO1 LOAD 與 ADC1_CH4 GPIO4 RPM 原始 12-bit 數值。 */
    uint16_t load_adc_raw = 0;
    uint16_t rpm_adc_raw = 0;
    bool load_adc_valid = false;
    bool rpm_adc_valid = false;
    bool mode_auto = false;
    bool low = false;
    bool adc_fault = false;
    uint8_t requested_page = 0;
    bool graph_selected = false; // [KEEP] Legacy field for existing UI/tests.
    uint8_t local_page = 0U; // [修改] 0: Main/Max, 1: Graph, 2: History, 3: IR TEST.
    uint16_t history[8] = {}; /* [修改] HISTORY 左右各 4 筆 */
    uint16_t history_peak = 0U; /* [新增] Flash 紀錄中的歷史最高值 */ // [ADD] Read-only snapshot from BBP storage.
    uint8_t history_count = 0U;
};
} // namespace brd_ui
#endif
