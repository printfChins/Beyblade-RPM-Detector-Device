/* 檔案位置: BRD_BPX_PRO/brd_ui_widgets.cpp
 * [修改] BLE WAIT 文字改成藍牙符號；置於電池左側。
 * [修改] 充電時僅保留電池內部流光，不顯示外部閃電。
 * [修改] MODE 顯示在底部，不再於底部重複顯示 BLE WAIT / LINK。
 */
#include "brd_config.h"
#include "brd_ui_screen.h"
#include "brd_ui_widgets.h"
namespace brd_ui {
namespace {
/* 頂部保留 x=0..111 給主畫面的裝載狀態。 */
constexpr Rect bluetooth_rect = {112, 1, 14, 16};
constexpr Rect battery_rect = {128, 2, 22, 14};
void draw_bluetooth(uint16_t color) {
    // [新增] GFX 線段組成標準藍牙符號，不依賴字型或自製 LCD 驅動。
    lcd().drawLine(117, 2, 117, 15, color);
    lcd().drawLine(117, 2, 122, 6, color);
    lcd().drawLine(122, 6, 114, 12, color);
    lcd().drawLine(114, 5, 122, 12, color);
    lcd().drawLine(122, 12, 117, 15, color);
}
void draw_battery(uint8_t percent, bool charging, uint8_t phase) {
    const uint8_t value = percent > 100U ? 100U : percent;
    const uint16_t color = charging ? palette::cyan :
        (value <= 20U ? palette::red : (value < 50U ? palette::yellow : palette::green));
    const uint16_t border = charging ? palette::cyan : palette::white;
    lcd().drawRect(129, 4, 17, 10, border);
    lcd().fillRect(146, 7, 3, 4, border);
    const int16_t width = (int16_t)((value * 13U + 50U) / 100U);
    if (width != 0) {
        lcd().fillRect(131, 6, width, 6, color);
        // [新增] 僅掃描真實電量已填滿的區域。
        if (charging) {
            const int16_t scan=(width<=2)?0:(int16_t)((phase%4U)*(width-2)/3);
            lcd().fillRect(131+scan, 6, (width-scan<2)?width-scan:2, 6, palette::white);
        }
    }
}
}
void header_static(const char *title) {
    lcd().fillRect(0, 0, TFT_WIDTH, 18, palette::panel);
    // [正式修改] 共用標題統一使用 GFX setTextSize(2) (12x16)。
    label(4, 1, title, 2, palette::white);
    lcd().drawFastHLine(0, 18, TFT_WIDTH, palette::blue);
}
bool header_battery(const Model &model, HeaderCache &cache) {
    bool changed = false;
    if (!cache.valid || cache.connected != model.connected || cache.disabled != model.ble_disabled) {
        clear_region(bluetooth_rect, palette::panel);
        const uint16_t color = model.ble_disabled ? palette::red :
            (model.connected ? palette::cyan : palette::gray);
        draw_bluetooth(color);
        changed = true;
    }
    const bool charge_changed = !cache.valid || cache.charging != model.charging ||
        (model.charging && cache.charge_phase != model.charge_phase);
    if (!cache.valid || cache.percent != model.battery || charge_changed) {
        clear_region(battery_rect, palette::panel);
        draw_battery(model.battery, model.charging, model.charge_phase);
        changed = true;
    }
    cache.percent = model.battery;
    cache.charging = model.charging;
    cache.charge_phase = model.charge_phase;
    cache.connected = model.connected;
    cache.disabled = model.ble_disabled;
    cache.valid = true;
    return changed;
}
void footer_static() {
    lcd().drawFastHLine(0, 68, TFT_WIDTH, palette::blue);
}
bool footer_update(const Model &model, FooterCache &cache, bool home_page) {
    // [正式修改][刪除] 首頁底部 MODE AUTO / MODE LOAD 顯示。
    // [保留] 量測模式本身與原有畫面切換邏輯不變。
    (void)home_page;
    cache.mode_auto = model.mode_auto;
    cache.valid = true;
    return false;
}
} // namespace brd_ui
