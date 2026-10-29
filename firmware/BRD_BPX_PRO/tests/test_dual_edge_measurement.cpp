/* 檔案: BRD_BPX_PRO/tests/test_dual_edge_measurement.cpp
 * [新增] CHANGE ISR 正負緣交錯事件：兩組獨立完整一圈、非半圈*2。
 */
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include <cassert>
#include <cstdio>
#include <vector>
#include "brd_config.h"
#include "brd_settings.h"
#include "brd_measurement.h"
#include "brd_shot_capture.h"
#include "brd_ui_curve.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
void brd_load_adc_begin() {}
void brd_load_adc_update() {}
bool brd_load_adc_active() { return false; }
const brd_settings::Values &brd_settings_get() {
    static const brd_settings::Values settings = {1U,1U,30U,2000U};
    return settings;
}
struct Period { uint32_t us; uint8_t edge; };
static std::vector<Period> captured;
void brd_bbp_capture_reset() { captured.clear(); }
void brd_bbp_capture_period(uint32_t us, uint32_t, uint8_t edge) { captured.push_back({us,edge}); }
void brd_bbp_capture_launch(uint32_t) {}
void brd_bbp_capture_finish() {}
void brd_bbp_capture_set_max(uint16_t) {}
void brd_bbp_capture_abort() {}
static void event(unsigned delta_us, int level) {
    test_clock_us += delta_us;
    GPIO.in.val = (GPIO.in.val & ~(1U << RPM_IR_GPIO)) |
                  (static_cast<uint32_t>(level) << RPM_IR_GPIO);
    assert(test_interrupts[RPM_IR_GPIO]);
    test_interrupts[RPM_IR_GPIO]();
    brd_measurement_update();
}
int main() {
    brd_measurement_begin();
    event(100U, HIGH);   // 正緣第 1 次
    event(2000U, LOW);   // 負緣第 1 次，尚未有完整一圈
    assert(captured.empty());
    event(2000U, HIGH);  // 正緣->正緣 = 4000us -> 15000 RPM
    assert(captured.empty()); /* [V1.4] 量測中禁止建立 BLE 曲線 */
    assert(brd_shot::diagnostics().recorded == 1U);
    assert(brd_measurement_get_display().value == 15000U);
    event(3000U, LOW);   // 負緣->負緣 = 5000us -> 12000 RPM
    assert(captured.empty());
    assert(brd_shot::diagnostics().recorded == 2U);
    assert(brd_measurement_get_display().value == 12000U);
    assert(brd_measurement_get_display().peak_value == 15000U);
    event(1000U, HIGH);  // 正緣->正緣 = 4000us -> 15000 RPM
    assert(captured.empty());
    assert(brd_shot::diagnostics().recorded == 3U);
    assert(brd_measurement_get_display().value == 15000U);
    event(40000U, LOW); /* 40ms 對負緣週期，低於 MAX 30% -> 完成 */
    assert(brd_measurement_get_display().show_max);
    assert(captured.empty());
    assert(brd_ui::curve_snapshot().count == 0U);
    assert(brd_shot::publish_finished());
    assert(!captured.empty());
    assert(brd_ui::curve_snapshot().frozen);
    puts("PASS: dual edges captured raw during measurement and BLE/TFT built only after finish.");
}
