/*
 * 檔案: BRD_BPX_PRO/brd_ui_input.cpp
 * [修改] GPIO5 INPUT_PULLUP 單鍵四頁循環：主畫面 -> 曲線 -> 歷史 -> IR TEST。
 * [修改] 短按放開後切換；按鍵消抖、長按及系統畫面期間不切頁。
 * [保留] GPIO 初始化由 brd_io.cpp 統一管理。
 */
#include <Arduino.h>
#include "brd_config.h"
#include "brd_io.h"
#include "brd_ui_input.h"

namespace brd_ui {
namespace {
bool raw_last = false;
bool stable_pressed = false;
bool press_allowed = false;
uint8_t local_page = 0U;
uint32_t changed_ms = 0U;
uint32_t pressed_ms = 0U;
}

void input_begin() {
    raw_last = brd_io_button_pressed();
    stable_pressed = raw_last;
    press_allowed = false;
    local_page = 0U;
    changed_ms = millis();
    pressed_ms = changed_ms;
}

bool input_update(bool allow_switch) {
    const bool raw_pressed = brd_io_button_pressed();
    const uint32_t now = millis();

    // [新增] 量測警告、BLE 指定 IR 或 OTA 接管期間，不允許延後觸發舊按鍵事件。
    if (!allow_switch) {
        press_allowed = false;
    }

    if (raw_pressed != raw_last) {
        raw_last = raw_pressed;
        changed_ms = now;
    }

    // [保留] 40 ms 防彈跳；必須保持穩定才接受狀態轉換。
    if (raw_pressed == stable_pressed ||
        (uint32_t)(now - changed_ms) < UI_BUTTON_DEBOUNCE_MS) {
        return false;
    }

    stable_pressed = raw_pressed;
    if (stable_pressed) {
        // [修改] 按下時只記錄起始時間；放開後才算一次完整短按。
        pressed_ms = now;
        press_allowed = allow_switch;
        return false;
    }

    // [新增] 僅接受完整短按；長按不切頁，也不會連續觸發。
    const bool short_press = press_allowed && allow_switch &&
        (uint32_t)(now - pressed_ms) <= UI_BUTTON_SHORT_PRESS_MAX_MS;
    press_allowed = false;
    if (!short_press) {
        return false;
    }

    // [修改] 原本 3 頁 -> 4 頁。
    local_page = (uint8_t)((local_page + 1U) % UI_LOCAL_PAGE_COUNT);
    return true;
}

bool input_graph_selected() {
    return local_page == 1U;
}

uint8_t input_current_page() {
    return local_page;
}
} // namespace brd_ui
