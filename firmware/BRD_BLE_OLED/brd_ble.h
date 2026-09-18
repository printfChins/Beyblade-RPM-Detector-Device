/*
    檔案位置: BRD_BLE_OLED/brd_ble.h
    [V1.15 修改] BLE 主迴圈工作與診斷；B1 每 200 ms 持續同步狀態。
*/
#ifndef BRD_BLE_H
#define BRD_BLE_H

#include <Arduino.h>

enum brd_ble_tx_state_t {
    BRD_BLE_TX_IDLE = 0,
    BRD_BLE_TX_START,
    BRD_BLE_TX_DATA,
    BRD_BLE_TX_END,
    BRD_BLE_TX_WAIT_ACK,
    BRD_BLE_TX_DONE,
    BRD_BLE_TX_TIMEOUT
};

struct brd_ble_diagnostics_t {
    bool initialized;
    bool connected;
    bool subscribed;
    bool reliable_mode;
    brd_ble_tx_state_t tx_state;
    uint32_t init_failures;
    uint32_t shutdown_failures;
    uint32_t notify_failures;
    uint32_t command_errors;
    uint32_t retransmissions;
    uint32_t ack_timeouts;
    /* [V1.11 新增] 分離記錄送出階段與 A4 STATUS 的逾時。 */
    uint32_t send_timeouts;
    uint32_t status_timeouts;
};

/* [新增] 低電 / ADC 故障時傳 false，停止廣播、連線並釋放 BLE stack。 */
void brd_ble_set_enabled(bool enabled);
/* [新增] 每次最多送一個 notification，不含 delay 或整條曲線傳送迴圈。 */
void brd_ble_update(void);
brd_ble_diagnostics_t brd_ble_get_diagnostics(void);
const char *brd_ble_get_device_name(void);
/* [V1.15 保留] 查詢 NimBLE host 是否停止；目前待機休眠已移除，低電關閉 BLE 仍可使用。 */
bool brd_ble_stack_stopped(void);

#endif
