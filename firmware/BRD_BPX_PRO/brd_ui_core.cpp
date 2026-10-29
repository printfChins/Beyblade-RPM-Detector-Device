/* 檔案: BRD_BPX_PRO/brd_ui_core.cpp
 * [新增] 新頁面只註冊路由與 enter/update；同頁不做全畫面清除。
 * [變更] 只有進入不同頁面時會使用 fillScreen() 清除背景。
 */
#include <Arduino.h>
#include "brd_config.h"
#include "brd_ui_screen.h"
#include "brd_ui_pages.h"
#include "brd_ui_core.h"
#include "brd_ui_navigation.h"
#include "brd_ui_motion.h"
#include "brd_ui_transition.h" // [ADD] GPIO5 four-page push-slide compositor.
#include "brd_ui_curve.h"
namespace brd_ui {
namespace {
bool is_ready = false;
Page active_page = Page::Boot;
bool valid_page = false;
void enter(Page page) {
    // [ADD] All forced pages cancel any in-progress manual navigation animation.
    transition::cancel();
    /* [新增] 唯一允許整頁清除的位置：第一次顯示或換頁。 */
    // [DELETE] Old global fillScreen; animated module clears exactly once on entry.
    if (motion::handles(page)) {
        motion::enter(page, millis());
        active_page = page;
        valid_page = true;
        return;
    }
    // [修改] OTA 使用離屏 GFX 畫布與 Tile Presenter；不先清空 LCD，避免瞬間黑屏。
    if (page != Page::Ota) lcd().fillScreen(palette::bg);
    switch (page) {
    case Page::Main: page_main::enter(); break;
    case Page::Graph: page_graph::enter(); break;
    case Page::IrTest: page_ir::enter(); break;
    case Page::Ota: page_ota::enter(); break;
    default: page_alert::enter(page); break;
    }
    active_page = page;
    valid_page = true;
}
}
bool begin() {
    is_ready = panel_begin();
    motion::begin(BRD_UI_MOTION_THEME);
    is_ready=is_ready && motion::ready();
    valid_page = false;
    return is_ready;
}
bool ready() { return is_ready; }
Page current_page() { return active_page; }
void show_boot() {
    if (!is_ready) { return; }
    enter(Page::Boot);
    // [修改] 開機畫面從第一幀開始渲染，後續由 animate_boot() 依 40ms 推進。
    motion::update(Page::Boot, Model{}, curve_snapshot(), millis());
    panel_backlight(true);
}
void animate_boot(uint32_t duration_ms) {
    if (!is_ready) { return; }
    // [修改] 保持原有開機等待時長；此階段尚未啟用量測及 BLE。
    const uint32_t started_at = millis();
    while ((uint32_t)(millis() - started_at) < duration_ms) {
        motion::update(Page::Boot, Model{}, curve_snapshot(), millis());
        delay(5U); // 僅在開機動畫使用，主迴圈 RPM 中斷未初始化。
    }
}
bool render(const Model &model) {
    if (!is_ready) { return false; }
    const Page requested = choose_display_page(model);
    const uint32_t now = millis();

    // [ADD] During a four-page slide, no other scene renderer may overwrite
    //       the outgoing canvas; the transition owns the framebuffer.
    if (transition::active()) {
        // [ADD] BLE-forced IR TEST is a system request even if the requested
        //       page ID equals the current manual IR TEST destination.
        const bool externally_forced_ir =
            model.requested_page == BRD_DISPLAY_PAGE_IR_TEST;
        if (requested == active_page && !externally_forced_ir) {
            const bool sent = transition::update(now);
            if (!transition::active()) {
                // [ADD] Last frame already contains the complete destination.
                //       No full-screen invalidate, hence no end-of-slide flash.
                motion::adopt_rendered(active_page, now);
            }
            return sent;
        }
        // [ADD] Low battery, ADC fault, OTA or any externally forced page
        //       preempts animation immediately.
        transition::cancel();
    }

    const bool entered = !valid_page || requested != active_page;
    if (entered && valid_page &&
        model.requested_page != BRD_DISPLAY_PAGE_IR_TEST &&
        transition::allowed(active_page, requested) &&
        transition::start(active_page, requested, motion::theme(),
                          model, curve_snapshot(), now)) {
        active_page = requested;
        valid_page = true;
        const bool sent = transition::update(now);
        if (!transition::active()) {
            motion::adopt_rendered(active_page, now);
        }
        panel_backlight(true);
        return sent;
    }

    if (entered) { enter(requested); }
    if (motion::handles(requested)) {
        const bool animated = motion::update(requested, model, curve_snapshot(), now);
        if (entered) { panel_backlight(true); }
        return entered || animated;
    }
    bool updated = false;
    switch (requested) {
    case Page::Main: updated = page_main::update(model); break;
    case Page::Graph: updated = page_graph::update(model); break;
    case Page::IrTest: updated = page_ir::update(model); break;
    case Page::Ota: updated = page_ota::update(model); break;
    default: break;
    }
    if (entered) { panel_backlight(true); }
    return entered || updated;
}
} // namespace brd_ui
