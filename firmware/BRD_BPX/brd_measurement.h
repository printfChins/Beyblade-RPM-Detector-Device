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
    /* [新增] 捕捉硬體初始化失敗時明確顯示 IR ERR。 */
    bool input_fault;
    bool sample_loss;
};

/* [V0.12 新增] 診斷計數由主 loop 更新，達 UINT32_MAX 後飽和。 */
struct brd_measurement_diagnostics_t {
    uint32_t load_queue_overflows;
    uint32_t rpm_queue_overflows;
    uint32_t load_stable_transitions;
    uint32_t launch_events;
    /* [新增] ISR 診斷均在 critical section 內讀寫，僅存固定 RAM。 */
    uint32_t rpm_isr_entries;
    uint32_t rpm_events_enqueued;
    uint32_t rpm_capture_disabled;
    uint32_t rpm_polarity_filtered;
    uint32_t rpm_events_dropped;
    uint32_t last_rpm_isr_us;
    uint32_t max_rpm_queue_depth;
    uint32_t capture_recoveries;
    uint32_t capture_init_failures;
    uint32_t invalid_measurements;
};

/* [V0.12 修改] 已啟用時呼叫不重置量測；供 ADC 故障恢復使用。 */
void brd_measurement_begin(void);
/* [V0.10 新增] 停用 RPM / LOAD 中斷並清除量測資料，供低電鎖定使用。 */
void brd_measurement_stop(void);
void brd_measurement_update(void);
/* [BRD_BBP 新增] 非量測時段才允許 Flash 保存及 BLE 初始化重試。 */
bool brd_measurement_storage_allowed(void);
/* [新增] 設定與清除紀錄只能在非量測安全時段執行。 */
bool brd_measurement_modifications_allowed(void);
/* [新增] Flash 交易暫停/恢復；不清除已完成的 MAX 與凍結曲線。 */
bool brd_measurement_pause_for_settings(void);
void brd_measurement_resume_after_settings(bool applied);
/* [新增] 原子套用已保存設定；若已有新事件就延後，不清除待處理脈衝。 */
bool brd_measurement_apply_settings(void (*apply)(void));
/* [新增] OTA 明確取得獨立維護暫停，一般 NVS 保存不停止捕捉。 */
bool brd_measurement_pause_for_ota(void);
void brd_measurement_resume_after_ota(void);
bool brd_measurement_background_ready(void);
brd_display_t brd_measurement_get_display(void);
brd_measurement_diagnostics_t brd_measurement_get_diagnostics(void);
void brd_measurement_max_frame_presented(uint32_t generation);

#endif
