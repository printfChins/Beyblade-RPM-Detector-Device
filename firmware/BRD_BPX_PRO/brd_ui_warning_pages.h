/*
 * File: BRD_BPX_PRO/brd_ui_warning_pages.h
 * [修改] 只保留 TORQUE LOW BATTERY / ADC FAULT 畫面模組。
 * [保留] ST7735 低階初始化及 SPI 傳送全部交由原本顯示模組處理。
 */
#ifndef BRD_UI_WARNING_PAGES_H
#define BRD_UI_WARNING_PAGES_H
#include <stdint.h>
#include <Adafruit_GFX.h>
namespace brd_ui { namespace warning_pages {
struct Palette {
    uint16_t background;
    uint16_t foreground;
    uint16_t accent;
    uint16_t muted;
    uint16_t warning;
    uint16_t danger;
};

enum class Kind : uint8_t {
    LowBattery = 0,
    AdcFault = 1
};

// [新增] 只繪圖，不擁有 LCD，不呼叫 SPI，不加入任何 delay。
// frame：低電頁只切換最後一格紅色電量；ADC頁保留原始警示點。
void draw(Adafruit_GFX &gfx, uint8_t theme, Kind kind, uint8_t frame,
          const Palette &palette);
} }
#endif
