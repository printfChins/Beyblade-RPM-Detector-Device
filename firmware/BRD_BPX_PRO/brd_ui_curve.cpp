/*
 * 檔案位置：BRD_BPX_PRO/brd_ui_curve.cpp
 * [修改 V1.4] 量測中不填點、不畫線；發射完成後一次發布摘要並凍結。
 * [保留] MAX 真實值及版本號通知，切頁不影響已完成結果。
 */
#include "brd_ui_curve.h"
namespace brd_ui {
namespace {
Curve points;
}
void curve_begin() { points = {}; }
void curve_capture_reset() {
    /* [修改] 新一發裝載才清除，保留逐次 version 供 Dirty Region 刷新。 */
    uint32_t next_version = points.version + 1U;
    points = {};
    points.version = next_version;
}
void curve_publish(const uint16_t *rpms, uint8_t count, uint16_t measured_peak) {
    if (rpms == nullptr || points.frozen) { return; }
    points.count = count > UI_CURVE_POINTS ? UI_CURVE_POINTS : count;
    for (uint8_t i = 0U; i < points.count; ++i) {
        points.rpm[i] = rpms[i];
    }
    points.peak = measured_peak;
    points.frozen = true;
    ++points.version;
}
void curve_observe(const brd_display_t &reading, uint32_t time_ms) {
    (void)reading;
    (void)time_ms;
}
const Curve &curve_snapshot() { return points; }
}
