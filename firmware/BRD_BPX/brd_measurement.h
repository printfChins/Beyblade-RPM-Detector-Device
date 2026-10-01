/*
    檔案位置: BRD_BBP/brd_measurement.h
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

/* [V0.12 新增] 診斷計數由主 loop 更新，達 UINT32_MAX 後飽和。 */
struct brd_measurement_diagnostics_t {
    uint32_t load_queue_overflows;
    uint32_t rpm_queue_overflows;
    uint32_t load_stable_transitions;
    uint32_t launch_events;
};

/* [V0.12 修改] 已啟用時呼叫不重置量測；供 ADC 故障恢復使用。 */
void brd_measurement_begin(void);
/* [V0.10 新增] 停用 RPM / LOAD 中斷並清除量測資料，供低電鎖定使用。 */
void brd_measurement_stop(void);
void brd_measurement_update(void);
/* [BRD_BBP 新增] 非量測時段才允許 Flash 保存及 BLE 初始化重試。 */
bool brd_measurement_storage_allowed(void);
brd_display_t brd_measurement_get_display(void);
brd_measurement_diagnostics_t brd_measurement_get_diagnostics(void);
void brd_measurement_max_frame_presented(uint32_t generation);

#endif
