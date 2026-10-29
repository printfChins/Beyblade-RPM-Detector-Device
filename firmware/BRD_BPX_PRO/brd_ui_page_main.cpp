/* BRD_BPX_PRO/brd_ui_page_main.cpp
 * [正式韌體修改] Unified 160x80 layout. No MODE, BTN, or empty footer.
 * [修改] Status header 2x, SP digits 3x, MAX label 2x.
 * [保留] Per-digit Dirty Region updates and measurement logic.
 */
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include "brd_config.h"
#include "brd_ui_screen.h"
#include "brd_ui_widgets.h"
#include "brd_ui_pages.h"
namespace brd_ui { namespace page_main {
namespace {
HeaderCache header_cache;
bool valid = false;
bool shown_max = false;
bool shown_hold = false;
bool shown_loaded = false;
bool shown_auto = false;
char shown_digits[12] = {};
uint8_t shown_scale = 0;
int16_t shown_x = 0;
int16_t shown_y = 0;
constexpr Rect load_rect = {0, 0, 112, 18};
constexpr Rect type_rect = {0, 21, 44, 52};
constexpr Rect value_rect = {44, 21, 116, 52};
const char *load_status(const Model &model) {
    if (model.measurement.hold_active) { return "HOLD"; }
    if (model.measurement.show_max) { return "SAVED"; }
    if (model.mode_auto) { return "READY"; }
    return model.measurement.loaded ? "LOADED" : "WAIT LOAD";
}
void draw_type(bool is_max) {
    clear_region(type_rect);
    // [正式修改] MAX 標籤改 setTextSize(2)，寬 36 px。
    if (is_max) { label(4, 24, "MAX", 2, palette::yellow); }
    label(4, is_max ? 46 : 39, "SP", 2, is_max ? palette::yellow : palette::cyan);
}
bool draw_value(const Model &model) {
    char digits[12];
    snprintf(digits, sizeof(digits), "%u", (unsigned)model.measurement.value);
    const size_t count = strlen(digits);
    // [正式修改] SP 數字 <= 6 位固定 3 號字；極端超長值才縮小避免超界。
    const uint8_t scale = count <= 6U ? 3U : (count <= 9U ? 2U : 1U);
    const int16_t x = 158 - text_width(digits, scale);
    const int16_t y = (scale == 3U ? 35 : (scale == 2U ? 39 : 43));
    const uint16_t color = model.measurement.show_max ? palette::yellow : palette::white;
    const bool compatible = valid && scale == shown_scale &&
        strlen(digits) == strlen(shown_digits) && x == shown_x &&
        y == shown_y && shown_max == model.measurement.show_max;
    if (compatible && strcmp(digits, shown_digits) == 0) { return false; }
    if (!compatible) {
        clear_region(value_rect);
        label(x, y, digits, scale, color);
    } else {
        for (size_t i = 0; digits[i] != '\0'; ++i) {
            if (digits[i] != shown_digits[i]) {
                label_opaque(x + (int16_t)(i * 6U * scale), y,
                             digits[i], scale, color, palette::bg);
            }
        }
    }
    strcpy(shown_digits, digits);
    shown_scale = scale;
    shown_x = x;
    shown_y = y;
    return true;
}
}
void enter() {
    lcd().fillRect(0, 0, TFT_WIDTH, 19, palette::panel);
    lcd().drawFastHLine(0, 19, TFT_WIDTH, palette::blue);
    lcd().drawFastHLine(4, 73, 152, palette::blue);
    header_cache = {};
    valid = false;
    shown_digits[0] = '\0';
    shown_scale = 0;
}
bool update(const Model &model) {
    bool changed = false;
    if (!valid || shown_max != model.measurement.show_max ||
        shown_hold != model.measurement.hold_active ||
        shown_loaded != model.measurement.loaded || shown_auto != model.mode_auto) {
        clear_region(load_rect, palette::panel);
        // [正式修改] LOADED / WAIT LOAD / HOLD / READY 使用 GFX 2 號字。
        label(4, 2, load_status(model), 2,
              model.measurement.hold_active ? palette::yellow :
              (model.measurement.loaded || model.mode_auto ? palette::cyan : palette::white));
        changed = true;
    }
    const bool is_max = model.measurement.show_max;
    if (!valid || shown_max != is_max) { draw_type(is_max); changed = true; }
    if (draw_value(model)) { changed = true; }
    if (header_battery(model, header_cache)) { changed = true; }
    shown_max = is_max;
    shown_hold = model.measurement.hold_active;
    shown_loaded = model.measurement.loaded;
    shown_auto = model.mode_auto;
    valid = true;
    return changed;
}
} } // namespace brd_ui::page_main
