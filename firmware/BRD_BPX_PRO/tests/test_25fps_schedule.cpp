/*
 * File: BRD_BPX_PRO/tests/test_25fps_schedule.cpp
 * [修改] 配合使用者現有設定，驗證 16 MHz / 40 ms 內層排程與 500 ms 低電動畫。
 * 主機端 mock，不代表實機 FPS。
 */
#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <cassert>
#include <cstdio>
#include "brd_config.h"
#include "brd_ui_motion.h"
#include "brd_ui_screen.h"
#include "brd_ui_curve.h"

uint64_t test_clock_us=0;
void (*test_interrupts[32])(void)={};
namespace brd_ui {
static Adafruit_ST7735 mock_lcd(&SPI,-1,2,-1);
Adafruit_ST7735& lcd(){return mock_lcd;}
}
int main() {
    static_assert(TFT_SPI_FREQUENCY_HZ == 16000000UL, "16 MHz expected");
    static_assert(BRD_TFT_POLL_INTERVAL_MS == 10U, "TFT poll interval expected to be 10 ms");
    static_assert(BRD_UI_MAIN_FRAME_INTERVAL_MS == 40U, "main interval expected to be 40 ms");
    static_assert(BRD_UI_GRAPH_FRAME_INTERVAL_MS == 40U, "graph interval expected to be 40 ms");
    static_assert(BRD_UI_HISTORY_FRAME_INTERVAL_MS == 40U, "history interval expected to be 40 ms");
    static_assert(BRD_UI_IR_FRAME_INTERVAL_MS == 40U, "IR test interval expected to be 40 ms");
    static_assert(BRD_UI_TRANSITION_STEP_INTERVAL_MS == 40U, "transition expected to be 40 ms");
    static_assert(BRD_UI_WARNING_FRAME_INTERVAL_MS == 500U, "low battery blink must remain 500 ms");
    brd_ui::Model model{};
    model.battery=84U;
    model.measurement.loaded=true;
    model.measurement.value=0U;
    model.measurement.peak_value=0U;
    brd_ui::Curve curve{};
    brd_ui::motion::begin(0U);
    assert(brd_ui::motion::ready());
    assert(brd_ui::motion::update(brd_ui::Page::Main,model,curve,0U));
    assert(!brd_ui::motion::update(brd_ui::Page::Main,model,curve,39U));
    assert(brd_ui::motion::update(brd_ui::Page::Main,model,curve,40U));
    assert(!brd_ui::motion::update(brd_ui::Page::Main,model,curve,79U));
    assert(brd_ui::motion::update(brd_ui::Page::Main,model,curve,80U));
    model.low=true;
    assert(brd_ui::motion::update(brd_ui::Page::LowBattery,model,curve,200U));
    assert(!brd_ui::motion::update(brd_ui::Page::LowBattery,model,curve,699U));
    assert(brd_ui::motion::update(brd_ui::Page::LowBattery,model,curve,700U));
    std::puts("PASS: 16 MHz SPI, main 40 ms update / 25 FPS target, 500 ms battery warning");
    return 0;
}
