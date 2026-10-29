/*
    檔案位置: BRD_BPX/brd_ota.h
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [V1.2 新增] OTA 主 loop、BLE 生命週期與 OLED 唯讀介面。
*/
#ifndef BRD_OTA_H
#define BRD_OTA_H
#include "brd_ota_protocol.h"
class NimBLEServer;
struct brd_ota_display_t {
    bool active, verifying;
    brd_ota::Phase phase;
    uint8_t percent;
};
bool brd_ota_create(NimBLEServer *server);
void brd_ota_connected(uint16_t handle);
void brd_ota_disconnected(uint16_t handle);
void brd_ota_stop(void);
void brd_ota_update(void);
bool brd_ota_is_busy(void);
brd_ota_display_t brd_ota_get_display(void);
#endif
