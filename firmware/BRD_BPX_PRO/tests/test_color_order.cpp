/* File: BRD_BPX_PRO/tests/test_color_order.cpp
 * [新增] 主機測試：確認 panel_begin 只更改 MADCTL 色序，不修改 TFT 旋轉與尺寸。
 */
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include "brd_config.h"
#include "brd_ui_screen.h"
uint64_t test_clock_us=0;
void (*test_interrupts[32])(void)={};
int main() {
    assert(brd_ui::panel_begin());
    auto &display=brd_ui::lcd();
    assert(display.width()==160 && display.height()==80);
#if TFT_PANEL_BGR_ORDER
    assert(display.madctl_write_count==1U);
    assert(display.last_madctl == (ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST7735_MADCTL_BGR));
#else
    assert(display.madctl_write_count==0U);
#endif
    std::puts("PASS: ST7735 RGB/BGR selectable; rotation=1, geometry=160x80");
}
