/*
 * File: BRD_BPX_PRO/brd_ui_page_alert.cpp
 * [修改] Boot：統一呼叫 style_scene，讓正式顯示與TORQUE 預覽共用繪圖程式。
 * [保留] 警告頁的後備顯示；正式警告由 motion::handles() 繪製。
 */
#include <Arduino.h>
#include "brd_config.h"
#include "brd_ui_screen.h"
#include "brd_ui_pages.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"
namespace brd_ui { namespace page_alert {
void enter(Page page) {
    if (page == Page::LowBattery) {
        lcd().fillRect(0,0,TFT_WIDTH,2,palette::red);
        label(6,4,"LOW BATTERY",2,palette::red);
        lcd().drawFastHLine(8,23,144,palette::blue);
        lcd().drawRect(14,29,48,29,palette::red);
        lcd().fillRect(62,37,5,13,palette::red);
        lcd().fillRect(19,34,6,19,palette::red);
        label(76,29,"CHARGE",2,palette::white);
        label(79,55,"POWER LOW",1,palette::yellow);
    } else if (page == Page::AdcFault) {
        lcd().fillRect(0,0,TFT_WIDTH,2,palette::red);
        label(6,4,"ADC ERROR",2,palette::red);
        lcd().drawFastHLine(8,23,144,palette::blue);
        label_center(28,"CHECK",2,palette::white);
        label_center(49,"BATTERY",2,palette::yellow);
        label_center(72,"ADC UNAVAILABLE",1,palette::gray);
    } else if (page == Page::Boot) {
        // [修改] Boot 最終場景對應預覽 GIF 的最後一幀。
        // [注意] 當前 Boot 是靜態畫面；GIF 展示的進場幀尚未接入啟動排程。
        const Model model{};
        const Curve curve{};
        style_scene::draw(lcd(),BRD_UI_MOTION_THEME,Page::Boot,7U,model,curve);
    }
}
} }
