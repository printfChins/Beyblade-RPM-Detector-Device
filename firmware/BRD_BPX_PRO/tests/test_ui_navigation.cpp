/*
 * 檔案: BRD_BPX_PRO/tests/test_ui_navigation.cpp
 * [新增] 四頁切換路由與低電／ADC／OTA／BLE 頁面優先權測試。
 */
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include "brd_ui_navigation.h"

int main() {
    brd_ui::Model state;
    using brd_ui::Page;
    using brd_ui::choose_display_page;
    assert(choose_display_page(state) == Page::Main);
    state.local_page = 1U;
    assert(choose_display_page(state) == Page::Graph);
    state.local_page = 2U;
    assert(choose_display_page(state) == Page::History);
    state.local_page = 3U;
    assert(choose_display_page(state) == Page::IrTest);
    state.local_page = 0U;
    state.measurement.show_max = true;
    // [修改] 量測完成仍使用 MAIN，不進入獨立 MAX 頁面。
    assert(choose_display_page(state) == Page::Main);
    state.local_page = 1U;
    assert(choose_display_page(state) == Page::Graph);
    state.local_page = 3U;
    assert(choose_display_page(state) == Page::IrTest);
    state.local_page = 2U;
    state.requested_page = BRD_DISPLAY_PAGE_IR_TEST;
    assert(choose_display_page(state) == Page::IrTest);
    state.ota.active = true;
    assert(choose_display_page(state) == Page::Ota);
    state.adc_fault = true;
    assert(choose_display_page(state) == Page::AdcFault);
    state.low = true;
    assert(choose_display_page(state) == Page::LowBattery);
    state.low = false;
    state.adc_fault = false;
    state.ota.active = false;
    state.requested_page = BRD_DISPLAY_PAGE_MAIN;
    assert(choose_display_page(state) == Page::History);
    puts("PASS: Main/Graph/History/IR routing and system priority");
    return 0;
}
