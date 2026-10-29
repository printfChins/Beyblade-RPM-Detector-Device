/* BRD_BPX_PRO/brd_ui_page_ir.cpp
 * [正式韌體修改] IR TEST layout proposal for 160x80 ST7735.
 * [修改] Removed bottom ADC legend/footer bar, enlarged title and bars.
 * [保留] Original sensor polygons, actual ADC values and Dirty Region separation.
 * 此檔案已整合進正式韌體。
 */
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include "brd_config.h"
#include "brd_ui_screen.h"
#include "brd_ui_widgets.h"
#include "brd_ui_pages.h"

namespace brd_ui { namespace page_ir {
namespace {
constexpr Rect k_voltage_rect = {98, 0, 62, 20};
constexpr Rect k_load_bar_rect = {0, 21, 33, 59};
constexpr Rect k_rpm_bar_rect = {127, 21, 33, 59};
constexpr Rect k_ir_load_rect = {42, 22, 36, 56};
constexpr Rect k_ir_rpm_rect = {91, 22, 32, 56};
const int8_t k_hexagon[6][2] = {{16,7},{12,14},{4,14},{0,7},{4,0},{12,0}};
const int8_t k_trapezoid[6][2] = {{0,4},{0,18},{10,22},{13,20},{13,2},{10,0}};
bool valid = false;
bool previous_ir1 = false;
bool previous_ir2 = false;
uint16_t previous_voltage_mv = 0;
uint8_t previous_battery_percent = 0;
uint16_t previous_load_raw = 0;
uint16_t previous_rpm_raw = 0;
bool previous_load_adc_valid = false;
bool previous_rpm_adc_valid = false;

void sensor_outline(int16_t x, int16_t y, const int8_t p[][2], size_t n,
                    uint8_t zoom, bool active) {
    const uint16_t color = active ? palette::white : palette::gray;
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1U) % n;
        lcd().drawLine(x + p[i][0] * zoom, y + p[i][1] * zoom,
                       x + p[j][0] * zoom, y + p[j][1] * zoom, color);
    }
    if (!active) { return; }
    for (size_t i = 1; i + 1U < n; ++i) {
        lcd().fillTriangle(x + p[0][0] * zoom, y + p[0][1] * zoom,
                           x + p[i][0] * zoom, y + p[i][1] * zoom,
                           x + p[i+1][0] * zoom, y + p[i+1][1] * zoom, color);
    }
}

void battery_voltage(uint16_t mv, uint8_t percent) {
    lcd().drawRect(100, 5, 18, 10, palette::white);
    lcd().fillRect(118, 8, 2, 4, palette::white);
    const uint8_t bounded = percent > 100U ? 100U : percent;
    const int16_t fill_width = (int16_t)(((uint32_t)bounded * 13U + 50U) / 100U);
    const uint16_t battery_color = bounded <= 20U ? palette::red :
        (bounded <= 50U ? palette::yellow : palette::cyan);
    if (fill_width > 0) { lcd().fillRect(102, 7, fill_width, 6, battery_color); }
    char value[10];
    const unsigned cv = ((unsigned)mv + 5U) / 10U;
    snprintf(value, sizeof(value), "%u.%02uV", cv / 100U, cv % 100U);
    label(127, 6, value, 1, palette::white);
}

void adc_bar(uint16_t raw, bool is_valid, bool load) {
    // [修改] 備援 IR TEST 顯示文字也與正式三風格一致：RPM 改為 SP。
    const int16_t x_text = load ? 4 : 141;
    const int16_t x_bar = load ? 9 : 137;
    const int16_t x_value = load ? 4 : 133;
    const uint16_t bar_color = load ? palette::cyan : palette::yellow;
    label(x_text, 23, load ? "LOAD" : "SP", 1, bar_color);
    lcd().drawRect(x_bar, 34, 14, 32, palette::gray);
    if (is_valid) {
        const uint32_t clipped = raw > 4095U ? 4095U : raw;
        const int16_t count = (int16_t)((clipped * 28U + 2047U) / 4095U);
        if (count > 0) {
            lcd().fillRect(x_bar + 2, 64 - count, 10, count, bar_color);
        }
    }
    char str[8];
    if (is_valid) { snprintf(str, sizeof(str), "%4u", (unsigned)raw); }
    else { snprintf(str, sizeof(str), "----"); }
    label(x_value, 70, str, 1, palette::white);
}
}

void enter() {
    /* [修改] Two-row layout: title/header 0-20; content from 21-79.
     *          Bottom footer line and caption are intentionally removed. */
    lcd().fillRect(0, 0, TFT_WIDTH, 20, palette::panel);
    label(4, 2, "IR TEST", 2, palette::white);
    lcd().drawFastHLine(0, 20, TFT_WIDTH, palette::blue);
    valid = false;
}

bool update(const Model &model) {
    bool changed = false;
    if (!valid || previous_voltage_mv != model.voltage_mv ||
        previous_battery_percent != model.battery) {
        clear_region(k_voltage_rect, palette::panel);
        battery_voltage(model.voltage_mv, model.battery);
        changed = true;
    }
    if (!valid || previous_ir2 != model.ir2) {
        clear_region(k_ir_load_rect);
        sensor_outline(44, 36, k_hexagon, 6, 2U, model.ir2);
        changed = true;
    }
    if (!valid || previous_ir1 != model.ir1) {
        clear_region(k_ir_rpm_rect);
        sensor_outline(93, 28, k_trapezoid, 6, 2U, model.ir1);
        changed = true;
    }
    if (!valid || previous_load_raw != model.load_adc_raw ||
        previous_load_adc_valid != model.load_adc_valid) {
        clear_region(k_load_bar_rect);
        adc_bar(model.load_adc_raw, model.load_adc_valid, true);
        changed = true;
    }
    if (!valid || previous_rpm_raw != model.rpm_adc_raw ||
        previous_rpm_adc_valid != model.rpm_adc_valid) {
        clear_region(k_rpm_bar_rect);
        adc_bar(model.rpm_adc_raw, model.rpm_adc_valid, false);
        changed = true;
    }
    previous_voltage_mv = model.voltage_mv;
    previous_battery_percent = model.battery;
    previous_ir1 = model.ir1;
    previous_ir2 = model.ir2;
    previous_load_raw = model.load_adc_raw;
    previous_rpm_raw = model.rpm_adc_raw;
    previous_load_adc_valid = model.load_adc_valid;
    previous_rpm_adc_valid = model.rpm_adc_valid;
    valid = true;
    return changed;
}
} } // namespace brd_ui::page_ir
