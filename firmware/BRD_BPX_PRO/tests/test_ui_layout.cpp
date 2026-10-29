/* 檔案位置: BRD_BPX_PRO/tests/test_ui_layout.cpp
 * [新增] 160x80 新版 UI 分區與 Dirty Region 測試。
 * [驗證] BLE / 電池 / 充電閃電獨立更新、數值全橫排、曲線完成後滿版。
 */
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_ui_core.h"
#include "brd_ui_curve.h"
#include "brd_ui_screen.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
bool brd_io_button_pressed() { return false; }
using namespace brd_ui;
static Model make_state() {
    Model s;
    s.measurement = {true, false, 30000U, 1U, false};
    s.battery = 80U;
    s.connected = true;
    s.mode_auto = false;
    return s;
}
static void assert_only_region_changed(const uint16_t *before, const Adafruit_ST7735 &tft,
                                       int x0, int y0, int x1, int y1) {
    for (int y = 0; y < 80; ++y) {
        for (int x = 0; x < 160; ++x) {
            if (x < x0 || x > x1 || y < y0 || y > y1) {
                assert(before[y * 160 + x] == tft.pixels[y * 160 + x]);
            }
        }
    }
}
int main() {
    assert(begin());
    Model m = make_state();
    assert(render(m));
    Adafruit_ST7735 &tft = lcd();
    assert(tft.full_clear_count == 1U);
    assert(tft.pixels[9 * 160 + 147] == palette::white); // 電池正極
    uint16_t original[160U * 80U];
    memcpy(original, tft.pixels, sizeof(original));

    // [新增] 斷開藍牙，只允許 BLE x=112..125 更新。
    m.connected = false;
    assert(render(m));
    assert_only_region_changed(original, tft, 112, 1, 125, 16);
    assert(tft.full_clear_count == 1U);
    memcpy(original, tft.pixels, sizeof(original));

    // [新增] 開始充電，只允許電池右側閃電 x=151..159 更新。
    m.charging = true;
    assert(render(m));
    assert_only_region_changed(original, tft, 151, 1, 159, 16);
    assert(tft.full_clear_count == 1U);
    assert(tft.pixels[9 * 160 + 152] == palette::yellow);
    memcpy(original, tft.pixels, sizeof(original));

    // [新增] LOAD 解除後頂部狀態更新，電池和 SP 必須保持原像素。
    m.measurement.loaded = false;
    assert(render(m));
    assert_only_region_changed(original, tft, 0, 0, 111, 17);
    memcpy(original, tft.pixels, sizeof(original));

    // [新增] 同位數更新只允許修改 160x80 中間數值區域。
    m.measurement.value = 30001U;
    assert(render(m));
    assert_only_region_changed(original, tft, 32, 20, 159, 65);
    assert(tft.full_clear_count == 1U);
    assert(!render(m));

    // [新增] 量測結束時不足 50 點，曲線仍應延伸至最右邊。
    curve_begin();
    for (uint8_t i = 0; i < 10U; ++i) {
        curve_observe({true, false, (uint16_t)(3000U + i * 1000U), 2U, false},
                      1000U + (uint32_t)i * 100U);
    }
    curve_observe({true, true, 13000U, 3U, true}, 2100U);
    assert(curve_snapshot().count == 10U && curve_snapshot().frozen);
    m.graph_selected = true;
    assert(render(m));
    assert(current_page() == Page::Graph);
    assert(tft.full_clear_count == 2U);
    // [新增] 曲線頁底部必須為左對齊 MAX SP + 5 位轉速，不能再顯示 LAST/PEAK。
    bool footer_label_yellow = false;
    bool footer_value_white = false;
    for (int y = 71; y <= 78; ++y) {
        for (int x = 4; x <= 44; ++x) {
            footer_label_yellow |= (tft.pixels[y * 160 + x] == palette::yellow);
        }
        for (int x = 46; x <= 85; ++x) {
            footer_value_white |= (tft.pixels[y * 160 + x] == palette::white);
        }
    }
    assert(footer_label_yellow && footer_value_white);

    bool rightmost_has_curve = false;
    for (int y = 24; y <= 68; ++y) { // [修改] 曲線新 Y 範圍
        if (tft.pixels[y * 160 + 157] == palette::cyan) { rightmost_has_curve = true; }
    }
    assert(rightmost_has_curve);
    const uint64_t last = tft.drawn_pixels;
    assert(!render(m));
    assert(tft.drawn_pixels == last);
    puts("PASS: BLE/battery/bolt separation, LOAD/header, SP full row, frozen curve full width");
}
