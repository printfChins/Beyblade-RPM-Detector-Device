/*
 * 檔案位置：BRD_BPX_PRO/brd_ui_curve.h
 * [修改 V1.4] TFT 僅接收發射完成後的 64 點摘要，量測中不得更新曲線。
 */
#ifndef BRD_UI_CURVE_H
#define BRD_UI_CURVE_H
#include <stdint.h>
#include "brd_config.h"
#include "brd_measurement.h"
namespace brd_ui {
struct Curve {
    uint16_t rpm[UI_CURVE_POINTS] = {};
    uint8_t count = 0U;
    uint16_t peak = 0U;
    bool frozen = false;
    uint32_t version = 0U;
};
void curve_begin();
void curve_capture_reset();
/* [新增] 僅由完成後的 postprocess 呼叫；非中斷/非量測路徑。 */
void curve_publish(const uint16_t *rpms, uint8_t count, uint16_t measured_peak);
void curve_observe(const brd_display_t &reading, uint32_t time_ms);
const Curve &curve_snapshot();
}
#endif
