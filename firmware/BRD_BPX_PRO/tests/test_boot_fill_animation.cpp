/* 檔案: BRD_BPX_PRO/tests/test_boot_fill_animation.cpp
 * [新增] 開機底部 8 格：由 0 格，逐 2 個 40ms 幀增加到 8 格。
 */
#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <cassert>
#include <cstdio>
#include "brd_ui_motion.h"
#include "brd_ui_screen.h"
#include "brd_ui_dirty_presenter.h"
uint64_t test_clock_us=0;
void (*test_interrupts[32])(void)={};
namespace brd_ui {
static Adafruit_ST7735 mock_lcd(&SPI,-1,2,-1);
Adafruit_ST7735& lcd(){return mock_lcd;}
}
static uint16_t rgb(int r,int g,int b) {
    return static_cast<uint16_t>(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static int lit_bars() {
    auto pixels=brd_ui::dirty_presenter::pixels();
    assert(pixels);
    int count=0;
    for(int i=0;i<8;++i) {
        if(pixels[68U*160U+68U+i*10U] == rgb(250,124,77)) { ++count; }
    }
    return count;
}
int main() {
    brd_ui::motion::begin(0U);
    brd_ui::Model m{};
    brd_ui::Curve curve{};
    brd_ui::motion::enter(brd_ui::Page::Boot,0U);
    assert(brd_ui::motion::update(brd_ui::Page::Boot,m,curve,0U));
    assert(lit_bars()==0);
    for(int frame=1;frame<=16;++frame) {
        const bool refreshed = brd_ui::motion::update(brd_ui::Page::Boot,m,curve,(uint32_t)frame*40U);
        assert(refreshed == ((frame % 2) == 0));
        assert(lit_bars()==frame/2);
    }
    assert(!brd_ui::motion::update(brd_ui::Page::Boot,m,curve,680U));
    puts("PASS: boot TORQUE bottom gauge animates 0 to 8/8 at 25 FPS frame scheduling.");
}
