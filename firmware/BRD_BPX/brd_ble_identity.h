/*
    檔案位置: BRD_BPX/brd_ble_identity.h
    [V1.4.4 新增] BLE 名稱的 NVS、安全寫入與廣播更新狀態。
    [保留] 所有 Flash 寫入均由主 loop 執行，ISR/callback 不碰 Flash。
*/
#ifndef BRD_BLE_IDENTITY_H
#define BRD_BLE_IDENTITY_H

#include <stddef.h>
#include <stdint.h>

enum brd_ble_name_status_t : uint8_t {
    BRD_BLE_NAME_IDLE = 0U,
    BRD_BLE_NAME_PENDING = 1U,
    BRD_BLE_NAME_SAVED = 2U,
    BRD_BLE_NAME_INVALID = 3U,
    BRD_BLE_NAME_BUSY = 4U,
    BRD_BLE_NAME_FLASH_ERROR = 5U
};

void brd_ble_identity_begin(void);
const char *brd_ble_identity_active_name(void);
void brd_ble_identity_read_name(char *out, size_t capacity);
brd_ble_name_status_t brd_ble_identity_request_name(const uint8_t *name, size_t length);
brd_ble_name_status_t brd_ble_identity_status(void);
void brd_ble_identity_update(void);
bool brd_ble_identity_needs_advertising_refresh(void);
void brd_ble_identity_clear_advertising_refresh(void);

#endif
