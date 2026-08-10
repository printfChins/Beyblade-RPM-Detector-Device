/*
    檔案名稱: brd_config.h
    檔案位置: BLE_RPM_V1.0/brd_config.h
*/

#ifndef BRD_CONFIG_H
#define BRD_CONFIG_H

#include <Arduino.h>

/* =========================
   專案名稱設定
   ========================= */

#define PROJECT_FULL_NAME                 "Beyblade RPM Detector"
#define PROJECT_SHORT_NAME                "BRD"
#define BLE_DEVICE_NAME_MAX_LEN           16

/* =========================
   LOG 開關設定
   ========================= */

#define LOG_MASTER                        (0)
#define LOG_SYSTEM                        (0)
#define LOG_BLE                           (0)
#define LOG_RECORD                        (0)
#define LOG_EDGE_TRIGGER                  (0)
#define LOG_RPM_REJECT                    (0)
#define LOG_RUNTIME                       (0)
#define LOG_CHARGE                        (0)
#define LOG_LOAD                          (0)
#define LOG_LED                           (0)

/* =========================
   GPIO 設定
   ========================= */

#define RPM_IR_GPIO                       0
#define RPM_IR_INPUT_MODE                 INPUT_PULLUP
#define RPM_IR_TRIGGER_EDGE               FALLING

#define LOAD_IR_GPIO                      1
#define LOAD_IR_INPUT_MODE                INPUT_PULLUP
#define LOAD_ACTIVE_LEVEL                 HIGH

#define CHRG_DET_GPIO                     10
#define CHRG_DET_INPUT_MODE               INPUT_PULLUP
#define CHRG_ACTIVE_LEVEL                 LOW

#define STATUS_LED_GPIO                   8
#define STATUS_LED_ACTIVE_LEVEL           LOW

#if (STATUS_LED_ACTIVE_LEVEL == HIGH)
#define STATUS_LED_INACTIVE_LEVEL         LOW
#else
#define STATUS_LED_INACTIVE_LEVEL         HIGH
#endif

/* =========================
   RPM 使用者可調參數
   ========================= */

#define PULSES_PER_REV                    1UL

#define RPM_IR_POLL_INTERVAL_US           50UL
#define RPM_IR_STABLE_TIME_US             100UL

#define LOAD_IR_POLL_INTERVAL_US          200UL
#define LOAD_IR_STABLE_TIME_US            1000UL

#define RPM_MIN_PERIOD_US                 500UL
#define RPM_MAX_PERIOD_US                 1000000UL
#define RPM_VALID_MAX                     60000UL

#define RPM_SAMPLE_INTERVAL_MS            50UL
#define RPM_ZERO_TIMEOUT_MS               300UL

#define STOP_RPM_THRESHOLD                0U
#define STOP_RPM_HOLD_MS                  150UL
#define POST_LAUNCH_NO_RPM_TIMEOUT_MS     1200UL
#define PRELAUNCH_IDLE_RESET_MS           3000UL

#define MIN_RECORD_SAMPLES                2U
#define MAX_RECORD_SAMPLES                1200U

#define CPU_FREQ_MHZ                      80

/* =========================
   LED 使用者可調參數
   ========================= */

#define LED_TRIGGER_RESTORE_MS            30UL

/* =========================
   BLE 設定
   ========================= */

#define BRD_SERVICE_UUID                  "7f510001-1b15-4d5f-9f4d-9b3c7a1d9a10"
#define BRD_NOTIFY_CHAR_UUID              "7f510002-1b15-4d5f-9f4d-9b3c7a1d9a10"

#define BLE_PKT_CURVE_START               0xA1
#define BLE_PKT_CURVE_DATA                0xA2
#define BLE_PKT_CURVE_END                 0xA3
#define BLE_PKT_LIVE                      0xB1
#define BLE_PKT_LAUNCH                    0xB2

#define BLE_SAMPLES_PER_PACKET            4U
#define BLE_PACKET_DELAY_MS               8UL
#define BLE_LIVE_INTERVAL_MS              50UL

#define LOG_INTERVAL_MS                   1000UL

#define INVALID_SAMPLE_INDEX              0xFFFFU

#endif
