/*
    檔案位置: BRD_OLED/brd_measurement.h
    [V0.10 修改] 僅公開單機量測與 OLED 畫面資料，不包含 BLE 狀態。
    [V0.10 新增] 畫面資料提供實際 HOLD 自鎖狀態。
*/
#ifndef BRD_MEASUREMENT_H
#define BRD_MEASUREMENT_H

#include <Arduino.h>

enum brd_state_t {
    BRD_STATE_WAIT_LOAD = 0,
    BRD_STATE_LOADED_READY,
    BRD_STATE_SPINNING_LOADED,
    BRD_STATE_SPINNING_LAUNCHED
};

struct brd_display_t {
    bool loaded;
    bool show_max;
    uint16_t value;
    uint32_t generation;
    /* [V0.10 新增] 僅 MAX 自鎖期間為 true；自鎖結束後保留 MAX 時為 false。 */
    bool hold_active;
};

void brd_measurement_begin(void);
/* [V0.10 新增] 停用 RPM / LOAD 中斷並清除量測資料，供低電鎖定使用。 */
void brd_measurement_stop(void);
void brd_measurement_update(void);
brd_display_t brd_measurement_get_display(void);
void brd_measurement_max_frame_presented(uint32_t generation);

#endif
