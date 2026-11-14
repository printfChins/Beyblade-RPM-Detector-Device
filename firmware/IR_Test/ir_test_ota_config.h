/*
 * 檔案：IR_Test_OTA/ir_test_ota_config.h
 * [新增] IR_Test 專用 BLE OTA 設定，不影響原 IR、ADC、OLED GPIO。
 */
#ifndef IR_TEST_OTA_CONFIG_H
#define IR_TEST_OTA_CONFIG_H

/* [沿用] BRD_BPX OTA V1.2 的既有 GATT UUID，與原 Web OTA 傳輸協定相容。 */
#define IR_OTA_SERVICE_UUID              "c7570010-9040-4c7b-87b1-086ef858ace7"
#define IR_OTA_CONTROL_UUID              "c7570011-9040-4c7b-87b1-086ef858ace7"
#define IR_OTA_DATA_UUID                 "c7570012-9040-4c7b-87b1-086ef858ace7"
#define IR_OTA_STATUS_UUID               "c7570013-9040-4c7b-87b1-086ef858ace7"

/*
 * [沿用] 原 Web Flash Download Tool 在 BLE 掃描時篩選這組 discovery UUID。
 * 只作 OTA 探索用的 GATT Service，不移植 BBP 指令協定。
 */
#define IR_OTA_DISCOVERY_SERVICE_UUID    "55c40000-f8eb-11ec-b939-0242ac120002"
#define IR_OTA_DISCOVERY_IDENTITY_UUID   "c7570014-9040-4c7b-87b1-086ef858ace7"

/* [沿用] 以原上位機可識別的 BLE 名稱前綴廣播，後面為 eFuse MAC 尾四碼。 */
#define IR_OTA_DEVICE_PREFIX             "BEYBLADE_TOOL_BRD_"
#define IR_OTA_MTU                       247U
#define IR_OTA_QUEUE_LENGTH              32U
#define IR_OTA_RESTART_DELAY_MS          5000UL

/*
 * [新增] IR_Test 沒有充電狀態與 SOC 曲線，改以分壓換算的電池電壓作保護。
 * 低於 3.65V 不允許開始 OTA（不是 SOC 20% 的精確等價值）。
 * 依實際電池與 ADC 校準調整；若感測電壓異常也拒絕開始。
 */
#define IR_OTA_MIN_BATTERY_MV             3650U
#define IR_OTA_MAX_BATTERY_MV             4400U

#endif
