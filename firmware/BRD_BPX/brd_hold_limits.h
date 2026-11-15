/*
    檔案位置: BRD_BPX/brd_hold_limits.h
    [V1.4.4 新增] HOLD 共用預設值/上下限，不依賴 Arduino 或 ESP-IDF。
    [保留] 原本 LOAD/AUTO HOLD 預設 2500ms。
*/
#ifndef BRD_HOLD_LIMITS_H
#define BRD_HOLD_LIMITS_H

#define HOLD_MIN_MS         1000U
#define HOLD_MAX_MS         10000U
#define HOLD_DEFAULT_MS     2500UL

#endif
