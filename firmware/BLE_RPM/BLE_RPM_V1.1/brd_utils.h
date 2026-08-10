/*
    檔案名稱: brd_utils.h
    檔案位置: BLE_RPM_V1.0/brd_utils.h

    集中放置共用轉換與文字化輔助函式。
*/

#ifndef BRD_UTILS_H
#define BRD_UTILS_H

#include <Arduino.h>

#include "brd_types.h"

uint16_t brd_limit_u32_to_u16(uint32_t value);
const char *brd_log_level_to_text(int level);
const char *brd_state_to_text(brd_state_t state);
void brd_write_u16_le(uint8_t *buf, uint16_t value);

#endif
