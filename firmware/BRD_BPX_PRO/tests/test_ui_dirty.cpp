/* 檔案: BRD_BPX_PRO/tests/test_ui_dirty.cpp
 * [新增] 確認相同資料不發出繪圖命令，更新 RPM/電池/IR/OTA 不全清螢幕。
 */
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_config.h"
#include "brd_ui_core.h"
#include "brd_ui_screen.h"
#include "brd_ui_curve.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
using namespace brd_ui;
bool brd_io_button_pressed() {return false;}
static Model default_state() {
    Model s;
    s.measurement = {true, false, 30000U, 1U, false};
    s.battery = 80U;
    s.voltage_mv = 4125U;
    s.connected = true;
    s.mode_auto = true;
    return s;
}
int main() {
    assert(begin());
    assert(ready());
    assert(lcd().init_count == 2U);
    show_boot();
    auto &tft = lcd();
    assert(tft.full_clear_count == 1U);
    Model s = default_state();
    assert(render(s));
    assert(current_page() == Page::Main);
    assert(tft.full_clear_count == 2U);
    uint64_t n = tft.drawn_pixels;
    assert(!render(s));
    assert(tft.drawn_pixels == n);
    uint16_t before[160U*80U];
    memcpy(before, tft.pixels, sizeof(before));
    s.measurement.value = 30001U;
    assert(render(s));
    assert(tft.full_clear_count == 2U);
    /* [新增] 同字數只變動 1 位：GFX 不透明字元重繪量應小於全螢幕。 */
    assert(tft.drawn_pixels - n < 1600U);
    for (int y=0;y<80;++y) for (int x=0;x<160;++x) {
        if (y<20 || y>=66 || x<32) {
            assert(before[y*160+x] == tft.pixels[y*160+x]);
        }
    }
    n = tft.drawn_pixels;
    s.battery = 60U;
    assert(render(s));
    assert(tft.full_clear_count == 2U);
    assert(tft.drawn_pixels - n < 2000U); // [修改] IR TEST 電池圖示及電壓標籤的局部重繪量較舊文字區大
    n = tft.drawn_pixels;
    s.mode_auto = false;
    assert(render(s));
    assert(tft.full_clear_count == 2U);
    assert(tft.drawn_pixels - n < 6500U);
    s.graph_selected = true;
    assert(render(s));
    assert(current_page() == Page::Graph);
    assert(tft.full_clear_count == 3U);
    assert(!render(s));
    assert(tft.full_clear_count == 3U);
    curve_observe({true,false,12300U,8U,false}, millis()+100U);
    n = tft.drawn_pixels;
    assert(render(s));
    assert(tft.full_clear_count == 3U);
    assert(tft.drawn_pixels-n < 11000U);
    assert(!render(s));
    curve_observe({true,false,15000U,8U,false}, millis()+220U);
    n = tft.drawn_pixels;
    assert(render(s));
    assert(tft.full_clear_count == 3U);
    assert(tft.drawn_pixels-n < 11000U);
    s.ota = {true, false, brd_ota::RECEIVING, 15U};
    assert(render(s));
    assert(current_page() == Page::Ota);
    assert(tft.full_clear_count == 4U);
    n = tft.drawn_pixels;
    s.ota.percent = 16U;
    assert(render(s));
    assert(tft.full_clear_count == 4U);
    assert(tft.drawn_pixels - n < 3000U);
    n = tft.drawn_pixels;
    assert(!render(s));
    assert(tft.drawn_pixels == n);
    s.ota.percent = 100U;
    assert(render(s));
    assert(tft.full_clear_count == 4U);
    s.ota.active = false;
    s.requested_page = BRD_DISPLAY_PAGE_IR_TEST;
    assert(render(s));
    assert(tft.full_clear_count == 5U);
    n = tft.drawn_pixels;
    s.ir1 = true;
    assert(render(s));
    assert(tft.full_clear_count == 5U);
    assert(tft.drawn_pixels - n < 3000U);
    n = tft.drawn_pixels;
    s.voltage_mv = 4050U;
    assert(render(s));
    assert(tft.full_clear_count == 5U);
    assert(tft.drawn_pixels - n < 2000U); // [修改] IR TEST 電池圖示及電壓標籤的局部重繪量較舊文字區大
    s.low = true;
    assert(render(s));
    assert(tft.full_clear_count == 6U);
    n = tft.drawn_pixels;
    assert(!render(s));
    assert(tft.drawn_pixels == n);
    puts("PASS: idle has zero GFX writes; per-widget Dirty updates; full clears only on page change");
}
