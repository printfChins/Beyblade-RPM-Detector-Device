/*
    檔案名稱: brd_ble.h
    檔案位置: BLE_RPM_V1.0/brd_ble.h
    
    BLE 名稱、GATT 初始化、即時資料、發射事件與曲線傳送模組介面。
*/

#ifndef BRD_BLE_H
#define BRD_BLE_H

void brd_ble_make_device_name(void);
void brd_ble_init(void);

void brd_ble_send_live_task(void);
void brd_ble_send_launch_event_task(void);
void brd_ble_send_result_task(void);

#endif
