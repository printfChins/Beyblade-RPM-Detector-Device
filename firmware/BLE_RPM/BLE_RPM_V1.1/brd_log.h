/*
    檔案名稱: brd_log.h
    檔案位置: BLE_RPM_V1.0/brd_log.h

    系統啟動資訊與週期 Runtime LOG 模組介面。
*/

#ifndef BRD_LOG_H
#define BRD_LOG_H

void brd_log_print_startup(void);
void brd_log_print_ready(void);
void brd_log_update_task(void);

#endif
