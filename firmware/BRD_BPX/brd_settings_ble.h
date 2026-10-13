/* 檔案位置: BRD_BPX/brd_settings_ble.h
   [新增] 設定 BLE 服務的建立、連線事件與主迴圈排程。 */
#ifndef BRD_SETTINGS_BLE_H
#define BRD_SETTINGS_BLE_H
#include <stdint.h>
class NimBLEServer;
bool brd_settings_ble_create(NimBLEServer *server);
void brd_settings_ble_connected(uint16_t handle);
void brd_settings_ble_disconnected(uint16_t handle);
void brd_settings_ble_stop(void);
void brd_settings_ble_update(void);
#endif
