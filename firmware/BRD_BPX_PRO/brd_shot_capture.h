/*
 * 檔案位置：BRD_BPX_PRO/brd_shot_capture.h
 * [新增 V1.4] RPM 優先：固定 RAM 原始紀錄，發射成功後才建立 UI/BLE 曲線。
 * [保留] GPIO3 ISR、正/負緣完整單圈計算與 MAX 判斷均在量測模組。
 */
#ifndef BRD_SHOT_CAPTURE_H
#define BRD_SHOT_CAPTURE_H

#include <stdint.h>

namespace brd_shot {

struct Diagnostics {
    uint16_t recorded = 0U;
    uint32_t dropped = 0U;
    uint16_t max_rpm = 0U;
    bool truncated = false;
    bool pending = false;
};

/* [新增] 只在主迴圈呼叫；ISR 僅保留時間戳與電位入列。 */
void reset();
void record(uint32_t period_us, uint32_t event_us, uint8_t edge_level, uint16_t rpm);
void finish(uint16_t measured_max_rpm);
void abort();
/* [新增] 只在完成狀態做曲線縮點；不得在 RPM 量測期間呼叫繪圖。 */
bool publish_finished();
Diagnostics diagnostics();

} // namespace brd_shot
#endif
