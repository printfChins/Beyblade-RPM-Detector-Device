/* BRD_BPX_PRO/tests/test_ui_ir_adc.cpp
 * [新增] IR TEST 左右 ADC、圖形與右上角電池的 Dirty Region 限界。
 */
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_config.h"
#include "brd_ui_core.h"
#include "brd_ui_screen.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
bool brd_io_button_pressed() { return false; }
using namespace brd_ui;
static void only_rect_changes(const uint16_t *before, int left,int top,int right,int bottom) {
    for (int y=0;y<80;++y) for (int x=0;x<160;++x) {
        if (x<left || x>right || y<top || y>bottom) {
            assert(before[y*160+x] == lcd().pixels[y*160+x]);
        }
    }
}
int main() {
    assert(begin());
    Model s;
    s.requested_page=BRD_DISPLAY_PAGE_IR_TEST;
    s.battery=80U;
    s.voltage_mv=4130U;
    s.load_adc_valid=true;
    s.rpm_adc_valid=true;
    s.load_adc_raw=1000U;
    s.rpm_adc_raw=2000U;
    assert(render(s));
    assert(current_page()==Page::IrTest);
    uint16_t before[160U*80U];
    memcpy(before,lcd().pixels,sizeof(before));
    assert(!render(s));
    s.load_adc_raw=2200U;
    assert(render(s));
    only_rect_changes(before,0,21,32,79); // [修改] LOAD ADC 新版縱向圖表 + 底部數值
    assert(lcd().full_clear_count==1U);
    memcpy(before,lcd().pixels,sizeof(before));
    s.rpm_adc_raw=3500U;
    assert(render(s));
    only_rect_changes(before,127,21,159,79); // [修改] RPM ADC 新版縱向圖表 + 底部數值
    memcpy(before,lcd().pixels,sizeof(before));
    s.ir1=true;
    assert(render(s));
    only_rect_changes(before,91,22,122,77); // [修改] 感測器圖形下移
    assert(lcd().pixels[42*160+112]==palette::white);
    memcpy(before,lcd().pixels,sizeof(before));
    s.voltage_mv=4070U;
    assert(render(s));
    only_rect_changes(before,98,0,159,19); // [修改] 電池+電壓新 header 區域
    memcpy(before,lcd().pixels,sizeof(before));
    s.battery=10U;
    assert(render(s));
    only_rect_changes(before,98,0,159,19); // [修改] 電池+電壓新 header 區域
    assert(lcd().full_clear_count==1U);
    memcpy(before,lcd().pixels,sizeof(before));
    s.rpm_adc_valid=false;
    assert(render(s));
    only_rect_changes(before,127,21,159,79); // [修改] RPM ADC 新版縱向圖表 + 底部數值
    assert(!render(s));
    puts("PASS: IR TEST LOAD/RPM ADC bars, battery voltage and sensor icons update only dirty rects.");
}
