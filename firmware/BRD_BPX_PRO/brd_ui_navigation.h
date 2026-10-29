/*
 * 檔案: BRD_BPX_PRO/brd_ui_navigation.h
 * [新增] 無硬體依賴的頁面路由規則，可單獨進行主機端測試。
 * [優先權] 低電量 > ADC 異常 > OTA > BLE 指定 IR TEST > GPIO5 本地頁面。
 */
#ifndef BRD_UI_NAVIGATION_H
#define BRD_UI_NAVIGATION_H

#include "brd_config.h"
#include "brd_ui_types.h"

namespace brd_ui {
inline Page choose_display_page(const Model &model) {
    if (model.low) {
        return Page::LowBattery;
    }
    if (model.adc_fault) {
        return Page::AdcFault;
    }
    if (model.ota.active) {
        return Page::Ota;
    }
    if (model.requested_page == BRD_DISPLAY_PAGE_IR_TEST) {
        return Page::IrTest;
    }

    // [修改] GPIO5: 0 Main (including MAX) -> 1 Graph -> 2 History -> 3 IR TEST。
    switch (model.local_page) {
    case 1U:
        return Page::Graph;
    case 2U:
        return Page::History;
    case 3U:
        return Page::IrTest;
    default:
        // [修改] MAX 不再切換獨立 Page::Max；由主畫面依 show_max 顯示。
        return Page::Main;
    }
}
} // namespace brd_ui

#endif
