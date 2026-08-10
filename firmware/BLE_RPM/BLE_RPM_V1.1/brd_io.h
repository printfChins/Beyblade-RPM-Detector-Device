/*
    檔案名稱: brd_io.h
    檔案位置: BLE_RPM_V1.0/brd_io.h
    
    GPIO8 狀態 LED 與 GPIO10 充電偵測模組介面。
*/

#ifndef BRD_IO_H
#define BRD_IO_H

void brd_io_begin(void);
void brd_charge_update_task(void);
void brd_status_led_on_rpm_trigger(void);
void brd_status_led_update_task(void);

#endif
