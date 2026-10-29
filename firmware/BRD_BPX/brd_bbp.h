/* 檔案位置: BRD_BPX/brd_bbp.h
   [BRD_BBP 新增] 完整新檔，API 不可由 ISR 呼叫。 */
#ifndef BRD_BBP_H
#define BRD_BBP_H
#include <stdint.h>
struct brd_bbp_diagnostics_t {
    uint32_t invalid_commands;
    uint32_t unknown_commands;
    uint32_t command_queue_overflows;
    uint32_t notify_failures;
    uint32_t storage_failures;
    uint32_t init_failures;
    /* [新增] GAP 回報原值，只有本裝置接受的主連線斷線才更新。 */
    uint32_t disconnect_events;
    int32_t last_disconnect_reason;
};
bool brd_bbp_begin(void);
void brd_bbp_stop(void);
bool brd_bbp_prepare_restart(void);
/* [V1.2 新增] 最後一筆結果發布並保存後，才允許 OTA 暫停量測。 */
bool brd_bbp_ota_ready(void);
void brd_bbp_update(bool storage_allowed);
/* [新增] 唯讀 BLE 啟動狀態，供 OLED 區分未連線與充電停用；只由主 loop 呼叫。 */
bool brd_bbp_is_started(void);
bool brd_bbp_is_connected(void);
brd_bbp_diagnostics_t brd_bbp_get_diagnostics(void);
void brd_bbp_capture_reset(void);
// [R3 修改] 非參考邊沿只更新代表 MAX，不增加曲線點數。
void brd_bbp_capture_period(uint32_t period_us, uint32_t event_us, bool record_profile = true);
// [R3 修改] 只標記發射；曲線從裝載後第一筆有效參考週期開始，不在發射時清除。
void brd_bbp_capture_launch(uint32_t launch_us);
void brd_bbp_capture_finish(void);
void brd_bbp_capture_abort(void);
#endif
