/* BRD_BPX_PRO/brd_ui_page_graph.cpp
 * [正式韌體修改] Graph takes the full usable width (x 3..157).
 * [修改] Graph y 24..68, MAX SP summary y71 without MODE/BTN.
 * [保留] Dirty Region incremental curve redraw.
 */
#include <Arduino.h>
#include <cstdio>
#include "brd_ui_curve.h"
#include "brd_ui_screen.h"
#include "brd_ui_pages.h"
#include "brd_ui_widgets.h"
namespace brd_ui { namespace page_graph {
namespace {
constexpr int16_t X0 = 3, X1 = 157, Y0 = 24, Y1 = 68;
constexpr Rect chart = {0, 21, 160, 48};
HeaderCache header_cache;
uint32_t shown_version = 0;
uint8_t shown_count = 0;
uint16_t shown_peak = 0;
bool shown_frozen = false;
bool valid = false;
int16_t graph_x(uint8_t index, uint8_t count, bool frozen) {
    const uint8_t steps = frozen && count > 1U ? (uint8_t)(count - 1U) :
        (uint8_t)(UI_CURVE_POINTS - 1U);
    return X0 + (int16_t)(((uint32_t)index * (X1 - X0) + steps / 2U) / steps);
}
int16_t graph_y(uint16_t rpm, uint16_t peak) {
    if (peak == 0U) { return Y1; }
    const uint32_t scaled = (uint32_t)rpm * (Y1 - Y0) / peak;
    return (int16_t)(Y1 - (scaled > (uint32_t)(Y1 - Y0) ? (Y1 - Y0) : scaled));
}
void grid() {
    for (int16_t y = Y0 + 11; y < Y1; y += 11) {
        lcd().drawFastHLine(X0, y, X1 - X0 + 1, palette::panel);
    }
    lcd().drawFastHLine(X0, Y1, X1 - X0 + 1, palette::gray);
    lcd().drawFastVLine(X0, Y0, Y1 - Y0 + 1, palette::gray);
}
void segment(const Curve &curve, uint8_t index) {
    const int16_t x = graph_x(index, curve.count, curve.frozen);
    const int16_t y = graph_y(curve.rpm[index], curve.peak);
    if (index == 0U) { lcd().fillCircle(x, y, 1, palette::cyan); }
    else {
        lcd().drawLine(graph_x((uint8_t)(index - 1U), curve.count, curve.frozen),
                       graph_y(curve.rpm[index - 1U], curve.peak), x, y, palette::cyan);
    }
}
}
void enter() {
    header_static("SP GRAPH");
    grid();
    header_cache = {};
    shown_count = 0;
    shown_peak = 0;
    shown_version = 0;
    shown_frozen = false;
    valid = false;
}
bool update(const Model &model) {
    const Curve &curve = curve_snapshot();
    bool changed = header_battery(model, header_cache);
    if (!valid || shown_version != curve.version) {
        const bool rescale = !valid || curve.count < shown_count ||
            curve.peak != shown_peak || curve.frozen != shown_frozen ||
            (curve.count == shown_count && curve.version != shown_version && !curve.frozen);
        if (rescale) {
            clear_region(chart);
            grid();
            for (uint8_t i = 0; i < curve.count; ++i) { segment(curve, i); }
        } else {
            for (uint8_t i = shown_count; i < curve.count; ++i) { segment(curve, i); }
        }
        clear_region({0, 70, 160, 10});
        // [正式修改] MAX SP 與數值組成一行，由 X=4 開始靠左對齊。
        // [保留] 僅重繪 y=70..79 的 Footer Dirty Region。
        label(4, 71, "MAX SP", 1, palette::yellow);
        char digits[14];
        snprintf(digits, sizeof(digits), "%u", (unsigned)curve.peak);
        label(46, 71, digits, 1, palette::white); // [修改] 數值緊接 MAX SP，不再右對齊。
        shown_version = curve.version;
        shown_count = curve.count;
        shown_peak = curve.peak;
        shown_frozen = curve.frozen;
        valid = true;
        changed = true;
    }
    return changed;
}
} } // namespace brd_ui::page_graph
