/*
 * 檔案: BRD_BPX_PRO/tests/test_ui_input_curve.cpp
 * [修改] 四頁短按、去抖、長按、系統頁屏蔽、曲線資料保留測試。
 */
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include "brd_config.h"
#include "brd_ui_input.h"
#include "brd_ui_curve.h"

uint64_t test_clock_us = 0;
void (*test_interrupts[32])(void) = {};
static bool pressed = false;
bool brd_io_button_pressed(void) { return pressed; }

static bool press_and_hold(uint32_t duration_ms, bool allowed) {
    pressed = true;
    assert(!brd_ui::input_update(allowed));
    delay(UI_BUTTON_DEBOUNCE_MS + 1U);
    assert(!brd_ui::input_update(allowed));
    delay(duration_ms);
    return brd_ui::input_update(allowed);
}

static bool release(bool allowed) {
    pressed = false;
    assert(!brd_ui::input_update(allowed));
    delay(UI_BUTTON_DEBOUNCE_MS + 1U);
    return brd_ui::input_update(allowed);
}

static void tap(uint8_t expected_page) {
    assert(!press_and_hold(90U, true));
    assert(brd_ui::input_current_page() == (uint8_t)((expected_page + 3U) % 4U));
    assert(release(true));
    assert(brd_ui::input_current_page() == expected_page);
    assert(!brd_ui::input_update(true));
}

int main() {
    brd_ui::input_begin();
    assert(!brd_ui::input_graph_selected());
    assert(brd_ui::input_current_page() == 0U);
    tap(1U); // Main -> Graph
    assert(brd_ui::input_graph_selected());
    tap(2U); // Graph -> History
    tap(3U); // History -> IR TEST
    assert(!brd_ui::input_graph_selected());
    tap(0U); // IR TEST -> Main

    // [新增] 800 ms 以上長按不切頁、不重複觸發。
    assert(!press_and_hold(UI_BUTTON_SHORT_PRESS_MAX_MS + 1U, true));
    assert(!release(true));
    assert(brd_ui::input_current_page() == 0U);

    // [新增] 系統頁阻止按鍵：按下後 OTA 進入，即使恢復也不得補觸發。
    assert(!press_and_hold(90U, true));
    assert(!brd_ui::input_update(false));
    assert(!release(true));
    assert(brd_ui::input_current_page() == 0U);

    // [新增] 系統頁期間完成一次短按不切頁。
    assert(!press_and_hold(90U, false));
    assert(!release(false));
    assert(brd_ui::input_current_page() == 0U);

    // [新增] 未達去抖門檻的雜訊不可切頁。
    pressed = true;
    assert(!brd_ui::input_update(true));
    delay(UI_BUTTON_DEBOUNCE_MS - 1U);
    pressed = false;
    assert(!brd_ui::input_update(true));
    delay(UI_BUTTON_DEBOUNCE_MS + 1U);
    assert(!brd_ui::input_update(true));
    assert(brd_ui::input_current_page() == 0U);

    brd_ui::curve_begin();
    /* [V1.4] 量測時資料保存在 Shot Capture，TFT Curve 不得提前顯示。 */
    assert(brd_ui::curve_snapshot().count == 0U);
    uint16_t result[] = {1000U, 2000U, 3000U, 4000U};
    brd_ui::curve_publish(result, 4U, 4000U);
    assert(brd_ui::curve_snapshot().count == 4U);
    assert(brd_ui::curve_snapshot().peak == 4000U);
    assert(brd_ui::curve_snapshot().frozen);
    brd_ui::curve_publish(result, 4U, 5000U);
    assert(brd_ui::curve_snapshot().peak == 4000U);
    brd_ui::curve_capture_reset();
    assert(brd_ui::curve_snapshot().count == 0U);
    assert(!brd_ui::curve_snapshot().frozen);
    printf("PASS GPIO5 4-page short-press navigation, debounce, priority block, and curve sampling\n");
}
