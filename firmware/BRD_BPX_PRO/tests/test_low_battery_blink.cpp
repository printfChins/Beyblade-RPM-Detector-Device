/*
 * File: BRD_BPX_PRO/tests/test_low_battery_blink.cpp
 * [新增] 低電頁連續閃爍、圖素變動範圍與 SPI 傳輸量測試。
 * [說明] 測試使用主機端 Adafruit GFX/ST7735 替身，不代表實機測試。
 */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "brd_ui_motion.h"
#include "brd_ui_dirty_presenter.h"
#include "brd_ui_screen.h"
#include "brd_ui_curve.h"

uint64_t test_clock_us=0;
void (*test_interrupts[32])(void)={};
namespace brd_ui {
static Adafruit_ST7735 mock_lcd(&SPI,-1,2,-1);
Adafruit_ST7735& lcd(){return mock_lcd;}
}

int main() {
    brd_ui::Model m{};
    m.low=true;
    brd_ui::Curve c{};
    brd_ui::motion::begin(0U);
    assert(brd_ui::motion::ready());

    constexpr int x=24; // TORQUE 主題電池最後一格，位於 x=23..27
    constexpr int y=37; // 電池內部固定位置
    const unsigned index=(unsigned)y*160U+(unsigned)x;
    std::vector<uint16_t> first;
    uint32_t count=0U;
    for(uint32_t time_ms=0U;time_ms<=12000U;time_ms+=500U) {
        const bool updated=brd_ui::motion::update(brd_ui::Page::LowBattery,m,c,time_ms);
        assert(updated);
        const uint16_t* pixels=brd_ui::lcd().pixels;
        if(time_ms==0U) {
            first.assign(pixels,pixels+160U*80U);
            assert(brd_ui::dirty_presenter::last_bytes()==25600U);
        } else {
            const unsigned frame=(unsigned)(time_ms/500U);
            const uint16_t expected=(frame%2U==0U)?first[index]:first[37U*160U+31U];
            assert(pixels[index]==expected);
            assert(brd_ui::dirty_presenter::last_bytes()<=1024U);
            for(int py=0;py<80;++py)for(int px=0;px<160;++px) {
                const unsigned k=(unsigned)py*160U+(unsigned)px;
                if(pixels[k]!=first[k]) {
                    assert(px>=23&&px<28);
                    assert(py>=36&&py<53);
                }
            }
        }
        const uint32_t before=brd_ui::lcd().transferred_bytes;
        assert(!brd_ui::motion::update(brd_ui::Page::LowBattery,m,c,time_ms+100U));
        assert(brd_ui::lcd().transferred_bytes==before);
        ++count;
    }
    assert(count==25U);
    std::puts("PASS: battery last bar 500ms blink, 12s persistence, Dirty Tile only.");
}
