/*
    [BRD_BBP 修改] 僅更新檔案路徑註解，原功能保留。
    檔案位置: BRD_BBP/brd_battery.h
    [V0.12 新增] ADC 狀態、錯誤碼、資料有效性與恢復計時診斷。
    所有介面由 setup / 主 loop 呼叫，不在 ISR 內使用。
*/
#ifndef BRD_BATTERY_H
#define BRD_BATTERY_H

#include <Arduino.h>
#include <esp_err.h>

enum brd_battery_status_t {
    BRD_BATTERY_NOT_SAMPLED = 0,
    BRD_BATTERY_OK,
    BRD_BATTERY_INIT_ERROR,
    BRD_BATTERY_READ_ERROR,
    BRD_BATTERY_CALIBRATION_ERROR,
    BRD_BATTERY_DATA_ERROR
};

struct brd_battery_diagnostics_t {
    brd_battery_status_t status;
    esp_err_t last_attempt_error;
    esp_err_t last_failure_error;
    uint32_t last_attempt_ms;
    uint32_t last_success_ms;
    uint32_t last_failure_ms;
    uint32_t success_count;
    uint32_t failure_count;
    uint32_t consecutive_failures;
    int last_valid_raw;
    int last_valid_adc_mv;
    bool last_sample_valid;
    bool has_valid_sample;
    bool adc_fault_active;
    bool recovery_pending;
    uint32_t recovery_elapsed_ms;
};

void brd_battery_begin(void);
void brd_battery_update(void);
/* [保留] 失敗時保存最後有效值；請配合診斷的 has_valid_sample 判斷。 */
uint16_t brd_battery_get_voltage_mv(void);
uint8_t brd_battery_get_percent(void);
bool brd_battery_is_low_locked(void);
bool brd_battery_restart_required(void);
/* [V0.12 新增] ADC 故障與低電鎖定分開；故障恢復無須等到 10%。 */
bool brd_battery_is_adc_fault(void);
bool brd_battery_measurement_allowed(void);
brd_battery_diagnostics_t brd_battery_get_diagnostics(void);

#endif
