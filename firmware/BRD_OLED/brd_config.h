/*
    檔案位置: BRD_OLED/brd_config.h
    [V0.10 修改] 單機 OLED 轉速版本。硬體腳位沿用附件 V1.9。
    [V0.10 刪減] BLE、休眠與曲線封包參數。
    [V0.10 恢復] 電池 ADC 取樣與 OLED 電量、充電圖示。
    [V0.10 新增] 低電鎖定、沒電警示及恢復後重新啟動。
*/
#ifndef BRD_CONFIG_H
#define BRD_CONFIG_H

#include <Arduino.h>
#include <esp_idf_version.h>
#include <hal/adc_types.h>

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "Select ESP32C3 Dev Module for this project."
#endif
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#error "This project requires Arduino-ESP32 with ESP-IDF 5.3 or newer."
#endif

#define PROJECT_FULL_NAME                 "Beyblade RPM Detector"
#define PROJECT_SHORT_NAME                "BRD"
#define PROJECT_VERSION                   "V0.12"
#define CPU_FIXED_FREQ_MHZ                80U
#define MAIN_LOOP_DELAY_MS                1UL

/* [V0.10 修改] RPM、LOAD 與 ADC 不啟用內部上下拉；充電 DET 例外使用上拉。 */
#define RPM_IR_GPIO                       3
#define RPM_IR_INPUT_MODE                 INPUT
#define RPM_IR_TRIGGER_EDGE               FALLING
#define LOAD_IR_GPIO                      1
#define LOAD_IR_INPUT_MODE                INPUT
#define LOAD_IR_TRIGGER_EDGE              CHANGE
#define LOAD_ACTIVE_LEVEL                 HIGH
#define BATTERY_ADC_GPIO                  0
#define CHRG_DET_GPIO                     10
/* [V0.10 修正] 充電 DET 沿用原電路 LOW=充電、滿電高阻，恢復內部上拉。 */
#define CHRG_DET_INPUT_MODE               INPUT_PULLUP
#define CHRG_ACTIVE_LEVEL                 LOW
#define STATUS_LED_GPIO                   8
#define STATUS_LED_INACTIVE_LEVEL         HIGH

/*
    [V0.10 恢復] 沿用 V1.9 電池量測設定。
    GPIO0 / A0，470k / 470k 分壓；12-bit、11 dB attenuation。
    [修改] 每 1 秒單次讀取 ADC mV 並直接換算，供低電即時判斷。
    不加入平均或 IIR 濾波。
*/
#define BATTERY_ADC_RESOLUTION_BITS       12U
/* [V0.12 修改] 原 ADC_11db 對應相同硬體衰減檔位，改用原生 IDF 名稱。 */
#define BATTERY_ADC_ATTENUATION           ADC_ATTEN_DB_12
#define BATTERY_DIVIDER_R_TOP_OHM         470000UL
#define BATTERY_DIVIDER_R_BOTTOM_OHM      470000UL
#define BATTERY_MIN_MV                   3300U
#define BATTERY_MAX_MV                   4200U
#define BATTERY_SAMPLE_INTERVAL_MS        1000UL
#define BATTERY_CALIBRATION_NUMERATOR     1000UL
#define BATTERY_CALIBRATION_DENOMINATOR   1000UL

/*
    [V0.10 新增] 低電鎖定門檻使用目前顯示的整數電量百分比。
    正常運作時: 電量 < 5% 立即鎖定，5% 不觸發。
    鎖定後: 電量 >= 10% 且通過恢復穩定時間才要求 MCU 軟體重啟。
    鎖定期間只保留 ADC、OLED 警示與必要的系統排程。
*/
/*
    [V0.12 新增] 原生 ADC 單次讀取，錯誤不寫入電壓與 SOC。
    從未取得有效值或有效資料已達 3 秒未更新: 暫停量測並顯示 ADC ERR。
    低電恢復: 每筆有效 SOC >= 10%，持續至少 3 秒才要求重啟。
    任一失敗、低於 10% 或取樣間隔超過 1.5 倍排程間隔，恢復計時歸零。
*/
#define BATTERY_ADC_STALE_TIMEOUT_MS      3000UL
#define BATTERY_RECOVER_STABLE_MS         3000UL
#define BATTERY_RECOVER_MAX_GAP_MS        (BATTERY_SAMPLE_INTERVAL_MS * 3UL / 2UL)
#define BATTERY_LOW_STOP_PERCENT          5U
#define BATTERY_RECOVER_PERCENT           10U
#define BATTERY_LOW_LOOP_DELAY_MS         20UL
#define BATTERY_LOW_OLED_REFRESH_MS       1000UL

/*
    [V0.10 新增] LOAD 去抖與 MAX 自鎖時間集中在同一區。
    去抖: 最後一次 LOAD 邊沿後，電位連續穩定的時間，單位 us。
    自鎖: MAX 完整畫面成功送出後至少保持的時間，單位 ms。
    OLED 故障時仍以量測完成時間作為解鎖備援，避免永久卡住。
    解鎖後重新讀取 LOAD 並重新去抖，忽略自鎖期間的歷史邊沿。
*/
/* [V0.12 新增] 環形佇列保留一格，32 格可保存 31 個 LOAD 邊沿。 */
#define LOAD_ISR_QUEUE_SIZE              32U
#define LOAD_IR_DEBOUNCE_US               1000UL
#define OLED_MAX_HOLD_MS                  2500UL

/* [保留] 一個 FALLING edge 等於一圈，從第二個有效 edge 計算 RPM。 */
#define PULSES_PER_REV                    1UL
#define RPM_ISR_QUEUE_SIZE               128U
#define RPM_MIN_PERIOD_US                500UL
#define RPM_MAX_PERIOD_US                1000000UL
#define RPM_VALID_MAX                    60000UL
#define RPM_ZERO_TIMEOUT_MS              300UL
#define PRELAUNCH_IDLE_RESET_MS           3000UL
#define POST_LAUNCH_NO_RPM_TIMEOUT_MS      1200UL
/* [V0.12 修改] MAX 的 50% 改為 20%，仍以單筆有效 RPM 判斷。 */
#define POST_LAUNCH_FINISH_PERCENT        20U

/*
    [V0.10 修改] 直接使用 ESP-IDF I2C master，初始化禁止內部上拉。
    SDA/SCL 需要使用電路或 OLED 模組既有的外部上拉電阻。
    [保留] 128x32 SSD1306、0x3C、400 kHz、原版 A0/C0 顯示方向。
*/
#define I2C_SDA_GPIO                      20
#define I2C_SCL_GPIO                      21
#define OLED_I2C_ADDRESS                  0x3CU
#define OLED_I2C_FREQUENCY_HZ             400000UL
#define OLED_I2C_TIMEOUT_MS               10
#define OLED_I2C_DATA_CHUNK_SIZE          16U
#define OLED_WIDTH                       128U
#define OLED_HEIGHT                      32U
#define OLED_UPDATE_INTERVAL_MS          100UL
#define OLED_BOOT_VERSION_DISPLAY_MS     1500UL
#define OLED_RETRY_INTERVAL_MS           1000UL

/* [V0.12 新增] ADC、事件佇列及計時參數的編譯期檢查。 */
static_assert(BATTERY_ADC_RESOLUTION_BITS == 12U, "ESP32-C3 ADC requires 12-bit configuration.");
static_assert(BATTERY_ADC_STALE_TIMEOUT_MS > BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_ADC_STALE_TIMEOUT_MS < 0x80000000UL, "Invalid ADC stale timeout.");
static_assert(BATTERY_RECOVER_STABLE_MS > 0UL && BATTERY_RECOVER_STABLE_MS < 0x80000000UL,
              "Invalid battery recovery duration.");
static_assert(BATTERY_SAMPLE_INTERVAL_MS < 0x80000000UL / 3UL &&
              BATTERY_RECOVER_MAX_GAP_MS >= BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_RECOVER_MAX_GAP_MS < BATTERY_ADC_STALE_TIMEOUT_MS, "Invalid recovery sample gap.");
static_assert(LOAD_ISR_QUEUE_SIZE >= 2U && LOAD_ISR_QUEUE_SIZE <= 256U,
              "LOAD queue size must be between 2 and 256.");
static_assert(RPM_ZERO_TIMEOUT_MS > 0UL && RPM_ZERO_TIMEOUT_MS < 0x80000000UL / 1000UL,
              "Invalid RPM zero timeout.");
static_assert(PRELAUNCH_IDLE_RESET_MS >= RPM_ZERO_TIMEOUT_MS &&
              PRELAUNCH_IDLE_RESET_MS < 0x80000000UL / 1000UL, "Invalid prelaunch idle timeout.");
static_assert(POST_LAUNCH_NO_RPM_TIMEOUT_MS > 0UL &&
              POST_LAUNCH_NO_RPM_TIMEOUT_MS < 0x80000000UL / 1000UL, "Invalid launch timeout.");
static_assert(RPM_IR_INPUT_MODE == INPUT, "RPM input must have no internal pull.");
static_assert(LOAD_IR_INPUT_MODE == INPUT, "LOAD input must have no internal pull.");
static_assert(CHRG_DET_INPUT_MODE == INPUT_PULLUP, "Charge DET must use an internal pull-up.");
static_assert(BATTERY_DIVIDER_R_BOTTOM_OHM > 0UL, "Battery divider denominator must be positive.");
static_assert(BATTERY_CALIBRATION_DENOMINATOR > 0UL, "Battery calibration denominator must be positive.");
static_assert(BATTERY_SAMPLE_INTERVAL_MS > 0UL && BATTERY_SAMPLE_INTERVAL_MS < 0x80000000UL,
              "Invalid battery sample interval.");
static_assert(BATTERY_LOW_STOP_PERCENT > 0U &&
              BATTERY_LOW_STOP_PERCENT < BATTERY_RECOVER_PERCENT &&
              BATTERY_RECOVER_PERCENT <= 100U,
              "Low battery thresholds must satisfy 0 < stop < recover <= 100.");
static_assert(BATTERY_LOW_LOOP_DELAY_MS > 0UL && BATTERY_LOW_LOOP_DELAY_MS <= 1000UL,
              "Invalid low battery loop delay.");
static_assert(BATTERY_LOW_OLED_REFRESH_MS > 0UL && BATTERY_LOW_OLED_REFRESH_MS < 0x80000000UL,
              "Invalid low battery OLED refresh interval.");
static_assert(RPM_IR_TRIGGER_EDGE == FALLING, "RPM must use FALLING edges.");
static_assert(PULSES_PER_REV == 1UL, "Each FALLING edge must equal one revolution.");
static_assert(RPM_ISR_QUEUE_SIZE >= 2U && RPM_ISR_QUEUE_SIZE <= 256U,
              "RPM queue size must be between 2 and 256.");
static_assert(RPM_VALID_MAX > 0UL && RPM_VALID_MAX <= 65535UL, "Invalid RPM limit.");
static_assert(POST_LAUNCH_FINISH_PERCENT > 0U && POST_LAUNCH_FINISH_PERCENT <= 100U,
              "Invalid finish percentage.");
static_assert(LOAD_IR_DEBOUNCE_US < 0x80000000UL, "LOAD debounce is too long.");
static_assert(OLED_MAX_HOLD_MS >= 2000UL && OLED_MAX_HOLD_MS < 0x80000000UL,
              "MAX display hold must be at least 2000 ms.");
static_assert(OLED_I2C_DATA_CHUNK_SIZE > 0U && OLED_I2C_DATA_CHUNK_SIZE <= 128U,
              "Invalid OLED I2C chunk size.");

#endif
