/*
    檔案位置: BRD_BPX/brd_config.h
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

#define PROJECT_FULL_NAME                 "Beyblade RPM Detector BattlePassX"
#define PROJECT_SHORT_NAME                "BRD"
/* [V1.3 修改] 韌體版本更新為 V1.3；[刪減] 原 V1.2 版本字串。 */
#define PROJECT_FIRMWARE_VERSION          "V1.4" /* [ME6211/100mA 修改] 電池曲線／FULL／3.46V 鎖定；保留 V1.4 版號 */
#define PROJECT_VERSION                   "BPX " PROJECT_FIRMWARE_VERSION
#define PROJECT_BOOT_NAME                 "BRD BPX"
#define PROJECT_BOOT_VERSION              "FW VER " PROJECT_FIRMWARE_VERSION
#define CPU_FIXED_FREQ_MHZ                80U
#define MAIN_LOOP_DELAY_MS                1UL

/*
    [修改] BLE 裝置名稱改為固定前綴 + ESP32 eFuse MAC 最後 2 bytes。
    實際名稱由 brd_bbp.cpp 在啟動時建立，例如 eFuse MAC 尾碼 0x12AF -> BEYBLADE_TOOL_BRD_12AF。
*/
#define BBP_DEVICE_NAME_PREFIX            "BEYBLADE_TOOL_BRD_"
#define BBP_SERVICE_UUID                  "55c40000-f8eb-11ec-b939-0242ac120002"
#define BBP_CHARACTERISTIC_UUID           "55c4f002-f8eb-11ec-b939-0242ac120002"
#define BBP_TX_POWER_DBM                  0
#define BBP_COMMAND_QUEUE_SIZE            32U
#define BBP_NOTIFY_INTERVAL_MS            10UL
/* [V1.4 BLE 30s 新增] A0 電量狀態封包的定期 Notify 間隔；不影響 12 頁曲線發送速率。 */
#define BBP_BATTERY_NOTIFY_INTERVAL_MS    30000UL
/* [新增] Notify 失敗只降低重試頻率，不中斷連線或 RPM。 */
#define BBP_NOTIFY_RETRY_INTERVAL_MS      100UL
/* [保留] 設定服務回覆的等待上限；BBP 分頁不再以此強制斷線。 */
#define BBP_NOTIFY_TIMEOUT_MS             1500UL
#define BBP_RETRY_INTERVAL_MS             1000UL
/*
    [BRD_BBP 修正 新增] 預設對應使用者確認可用的 V0.1 主動通知流程。
    1: 訂閱後先送 A0，之後每 30 秒自動送 A0；不再由 LOAD 改變觸發。
       發射紀錄完成送 B0~B7 / 70~73。
       A0 bit2 表示去抖後的 LOAD，與該可用版本一致。
    0: 嚴格依 battlepass-emulation.md，僅回覆 0x51 / 0x74；
       bit2 仍表示有效轉動至結果發布前，不表示靜態 LOAD。
    [刪減] 不再把純輪詢設為所有上位機共用的唯一流程。
*/
#ifndef BBP_COMPAT_AUTONOTIFY
#define BBP_COMPAT_AUTONOTIFY              1
#endif
#define BBP_SUBSCRIBE_SETTLE_MS            100UL
static_assert(BBP_COMPAT_AUTONOTIFY == 0 || BBP_COMPAT_AUTONOTIFY == 1,
              "BBP compatibility mode must be 0 or 1.");
/* [新增] 有效轉動起的發布下限；量測尚未結束時仍繼續等待。 */
#define BBP_PUBLISH_MIN_DELAY_US          600000UL
/* [新增] BRD 鋰電池 SOC 門檻，非聲稱已確認原廠 AAA 門檻。 */
#define BBP_BATTERY_WARNING_PERCENT       20U
#define BBP_BATTERY_CRITICAL_PERCENT      5U
/* [新增] BRD 設定 API 獨立於既有 BBP 封包；下列為編譯期結構參數。 */
#define SETTINGS_SERVICE_UUID            "c7570001-9040-4c7b-87b1-086ef858ace7"
/* [V1.2 新增] 獨立 OTA UUID；不更動既有 BBP / 設定服務。 */
#define OTA_SERVICE_UUID                 "c7570010-9040-4c7b-87b1-086ef858ace7"
#define OTA_CONTROL_UUID                 "c7570011-9040-4c7b-87b1-086ef858ace7"
#define OTA_DATA_UUID                    "c7570012-9040-4c7b-87b1-086ef858ace7"
#define OTA_STATUS_UUID                  "c7570013-9040-4c7b-87b1-086ef858ace7"
#define OTA_MIN_BATTERY_PERCENT          20U
/* [V1.2 修改] 8 筆 burst + 控制命令的接收緩衝，滿列不覆寫。 */
#define OTA_COMMAND_QUEUE_SIZE           32U
/* [V1.2 新增] 實際 GATT Payload 仍由協商後 MTU 決定。 */
#define OTA_PREFERRED_MTU                 247U
#define OTA_RESTART_DELAY_MS             5000UL
#define SETTINGS_REQUEST_UUID            "c7570002-9040-4c7b-87b1-086ef858ace7"
#define SETTINGS_RESPONSE_UUID           "c7570003-9040-4c7b-87b1-086ef858ace7"
/* [新增] 獨立 1-byte OLED 頁面 BLE characteristic，不改舊設定封包與 NVS。 */
#define SETTINGS_DISPLAY_PAGE_UUID       "c7570004-9040-4c7b-87b1-086ef858ace7"
#define BRD_DISPLAY_PAGE_MAIN            0U
#define BRD_DISPLAY_PAGE_IR_TEST         1U
/* [新增] IR TEST 觸發 HIGH 為實心，沿用原 IR_Test 的觸發邏輯。 */
#define IR_TEST_TRIGGER_LEVEL            HIGH
#define SETTINGS_COMMAND_QUEUE_SIZE       8U
#define SETTINGS_NVS_NAMESPACE            "brd_cfg"
#define SETTINGS_NVS_KEY                  "settings_v1"
#define SETTINGS_WRITE_INTERVAL_MS        1000UL
#define SETTINGS_CHARGE_CLOSE_GRACE_MS    200UL
/* [修改] 預設停用充電關閉 BLE：0=充電時保留 BLE；1=充電時關閉 BLE。
   [刪減] 原預設啟用值 1U。可透過設定 API 覆寫，已保存的 Flash 設定優先。 */
#define BLE_OFF_WHILE_CHARGING            0U
#define BBP_NVS_NAMESPACE                 "brd_bbp"
#define BBP_NVS_KEY                       "history_v1"
static_assert(BBP_COMMAND_QUEUE_SIZE > 0U && BBP_COMMAND_QUEUE_SIZE <= 256U,
              "Invalid BBP command queue size.");
static_assert(BBP_NOTIFY_INTERVAL_MS > 0UL && BBP_NOTIFY_INTERVAL_MS < 40UL,
              "12 BBP pages must fit inside a 500ms poll interval.");
/* [V1.4 BLE 30s 新增] 定期 A0 只能用獨立週期，不得覆蓋分頁 Notify 間隔。 */
static_assert(BBP_BATTERY_NOTIFY_INTERVAL_MS >= BBP_SUBSCRIBE_SETTLE_MS &&
              BBP_BATTERY_NOTIFY_INTERVAL_MS < 0x80000000UL,
              "Invalid periodic BBP battery Notify interval.");
static_assert(BBP_NOTIFY_RETRY_INTERVAL_MS >= BBP_NOTIFY_INTERVAL_MS && BBP_NOTIFY_RETRY_INTERVAL_MS < 0x80000000UL,
              "Invalid BBP Notify retry interval.");
static_assert(BBP_PUBLISH_MIN_DELAY_US < 2000000UL,
              "Publish must fit inside OLED hold interval.");
static_assert(BBP_BATTERY_CRITICAL_PERCENT <= BBP_BATTERY_WARNING_PERCENT &&
              BBP_BATTERY_WARNING_PERCENT <= 100U, "Invalid BBP battery thresholds.");

/* [V0.10 修改] RPM、LOAD 與 ADC 不啟用內部上下拉；充電 DET 例外使用上拉。 */
#define RPM_IR_GPIO                       3
#define RPM_IR_INPUT_MODE                 INPUT
/* [修改] RPM 使用 CHANGE 捕捉正、負緣；本次量測第一個觸發邊沿決定固定參考極性。
    先正緣：正緣 -> 正緣計算一圈 RPM；先負緣：負緣 -> 負緣計算一圈 RPM。 */
#define RPM_IR_TRIGGER_EDGE               CHANGE
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
#define BATTERY_MIN_MV                   BATTERY_LOW_LOCK_MV /* [ME6211/100mA 修改] 3.46V 為可用容量 0% */
#define BATTERY_MAX_MV                   4200U
#define BATTERY_SAMPLE_INTERVAL_MS        1000UL
/* [V1.4 本次修改] 200mAh 電池／100mA 充電電流，理想 CC 充電每 1% 所需時間：
   (200mAh x 1% / 100mA) x 3600000ms/h = 72000ms。
   [刪減] 固定每 60 秒增加 1%（等效 120mA，較充電設定偏快）。
   [新增] 由容量與電流自動推導上升間隔；插線不跳升、未 FULL 99% 上限不變。
   注意：此處使用標稱充電電流計算顯示速率上限，非庫倫計量；充電後段 CV
   電流降低及充電效率可能使真實 SOC 增長較慢。FULL 仍沿用既有電壓判定。 */
#define BATTERY_NOMINAL_CAPACITY_MAH      200U
#define BATTERY_CHARGE_CURRENT_MA        100U
#define BATTERY_CHARGE_SOC_RISE_INTERVAL_MS \
    ((36000UL * BATTERY_NOMINAL_CAPACITY_MAH + BATTERY_CHARGE_CURRENT_MA / 2UL) / \
     BATTERY_CHARGE_CURRENT_MA)
/* [V1.4 充電 SOC 新增] 充電偵測轉為非充電，至少穩定 3 秒才切回放電 SOC。 */
#define BATTERY_CHARGE_DET_EXIT_CONFIRM_MS 3000UL
#define BATTERY_CALIBRATION_NUMERATOR     1000UL
#define BATTERY_CALIBRATION_DENOMINATOR   1000UL

/*
    [ME6211/100mA 修改] 100mA 典型 dropout=120mV；3.3V+0.12V=3.42V 參考電壓。
    [修改] 以實測 CSV 於 3.42V 前約 7.82 分鐘的 3.46V 設為 0%/警告。
    [修改] <=3.46V 持續 2 秒鎖定；<=3.42V 立即鎖定（典型壓差參考）。
    [保留] 鎖定後必須曾偵測到充電，>=3.65V 穩定 30 秒才能重啟。
    [保留] 充電中 >=4.15V 穩定 30 秒確認 FULL，否則最高 99%。
    [保留] ADC 1s 單次讀值、失敗保留最後有效數據、OLED 警告與 RPM 優先權。
    注意：ME6211 dropout 是典型值，若要最低輸出保證需實測 LDO VOUT。
*/
#define BATTERY_ADC_STALE_TIMEOUT_MS      3000UL
#define BATTERY_RECOVER_MAX_GAP_MS        (BATTERY_SAMPLE_INTERVAL_MS * 3UL / 2UL)
#define BATTERY_LOW_LOCK_MV               3460U
#define BATTERY_LOW_HARD_STOP_MV          3420U
#define BATTERY_LOW_CONFIRM_MS            2000UL
#define BATTERY_RECOVER_MV                3650U
#define BATTERY_RECOVER_STABLE_MS         30000UL
#define BATTERY_FULL_CONFIRM_MV           4150U
#define BATTERY_FULL_CONFIRM_MS           30000UL
#define BATTERY_FULL_RELEASE_MV           4100U
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
#define LOAD_IR_DEBOUNCE_US             50000UL
#define OLED_MAX_HOLD_MS                  2500UL

/*
    [新增] 量測模式選擇。
    BRD_MEASUREMENT_MODE_LOAD:
        LOAD 穩定 HIGH 達去抖時間（預設 50 ms）後清零並啟動 RPM 檢測；RPM 開始後 LOAD 不參與發射判定。
        發射成功依即時 RPM 嚴格低於本次 MAX 的執行時設定百分比判定。
    BRD_MEASUREMENT_MODE_AUTO:
        不使用 LOAD 作為開始 / 發射 / 歸零條件，只依 RPM 自動判定。
        AUTO_RPM_THRESHOLD 為唯一 RPM 門檻，目前設定 2000 RPM。
        有效發射: RPM 曾大於等於門檻，之後即時 RPM 嚴格低於 MAX 的設定百分比。
        有效歸零: RPM 小於門檻，之後無有效 RPM 持續至少 250 ms。
        達門檻後共用 RPM_ZERO_TIMEOUT_MS 的無脈衝歸零備援，立即封存有效結果。
*/
#define BRD_MEASUREMENT_MODE_LOAD         0U
#define BRD_MEASUREMENT_MODE_AUTO         1U
#ifndef BRD_MEASUREMENT_MODE
#define BRD_MEASUREMENT_MODE              BRD_MEASUREMENT_MODE_LOAD
#endif
#define AUTO_RPM_THRESHOLD                2000U
/* [刪減] AUTO_LAUNCH_ZERO_MS：AUTO 不再等待零轉速滿一秒，改共用百分比結束。 */
#define AUTO_RESET_ZERO_MS                250UL

/* [修改] 選定參考極性的相鄰同極性邊沿為一圈，因此每一有效週期直接換算一次 RPM。 */
#define PULSES_PER_REV                    1UL
/* [修改] IRAM ISR 持續收取背景 Flash 期間的事件，1023 筆約容納 2 秒的 30000 RPM。 */
#define RPM_ISR_QUEUE_SIZE               1024U
#define RPM_SNAPSHOT_CHUNK_SIZE           16U
#define RPM_MIN_PERIOD_US                500UL
#define RPM_MAX_PERIOD_US                1000000UL
#define RPM_VALID_MAX                    60000UL
#define RPM_ZERO_TIMEOUT_MS              300UL
#define PRELAUNCH_IDLE_RESET_MS           3000UL
#define POST_LAUNCH_NO_RPM_TIMEOUT_MS      1200UL
/* [修改] 兩模式共用預設 30%；BLE API 可以修改為 1..100%。 */
#define POST_LAUNCH_FINISH_PERCENT        30U

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
static_assert(BATTERY_CHARGE_DET_EXIT_CONFIRM_MS >= BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_CHARGE_DET_EXIT_CONFIRM_MS < 0x80000000UL,
              "Invalid charge-det exit debounce interval.");
/* [V1.4 本次新增] 防止容量／電流設定錯誤，避免除零與整數溢位。 */
static_assert(BATTERY_NOMINAL_CAPACITY_MAH > 0U && BATTERY_NOMINAL_CAPACITY_MAH <= 1000U &&
              BATTERY_CHARGE_CURRENT_MA > 0U && BATTERY_CHARGE_CURRENT_MA <= 2000U,
              "Invalid battery capacity or charge current.");
static_assert(BATTERY_CHARGE_SOC_RISE_INTERVAL_MS >= BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_CHARGE_SOC_RISE_INTERVAL_MS < 0x80000000UL,
              "Invalid charge SOC rise interval.");
static_assert(BATTERY_ADC_STALE_TIMEOUT_MS > BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_ADC_STALE_TIMEOUT_MS < 0x80000000UL, "Invalid ADC stale timeout.");
static_assert(BATTERY_RECOVER_STABLE_MS > 0UL && BATTERY_RECOVER_STABLE_MS < 0x80000000UL,
              "Invalid battery recovery duration.");
static_assert(BATTERY_SAMPLE_INTERVAL_MS < 0x80000000UL / 3UL &&
              BATTERY_RECOVER_MAX_GAP_MS >= BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_RECOVER_MAX_GAP_MS < BATTERY_ADC_STALE_TIMEOUT_MS, "Invalid recovery sample gap.");
static_assert(LOAD_ISR_QUEUE_SIZE >= 2U && LOAD_ISR_QUEUE_SIZE <= 256U,
              "LOAD queue size must be between 2 and 256.");
static_assert(BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD ||
              BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_AUTO,
              "Invalid BRD measurement mode.");
static_assert(AUTO_RPM_THRESHOLD > 0U && AUTO_RPM_THRESHOLD < RPM_VALID_MAX,
              "Invalid AUTO RPM threshold.");
static_assert(AUTO_RESET_ZERO_MS > 0UL &&
              AUTO_RESET_ZERO_MS < 0x80000000UL / 1000UL,
              "Invalid AUTO zero timing.");
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
/* [V1.4 修改] 以實測電壓管理 0%、FULL、恢復，刪除百分比門檻 assert。 */
static_assert(BATTERY_LOW_HARD_STOP_MV < BATTERY_LOW_LOCK_MV &&
              BATTERY_LOW_LOCK_MV < BATTERY_RECOVER_MV && BATTERY_RECOVER_MV < BATTERY_MAX_MV,
              "Battery cutoff/recovery voltage order invalid.");
static_assert(BATTERY_MIN_MV == BATTERY_LOW_LOCK_MV,
              "Battery SOC zero must equal low-battery warning threshold.");
static_assert(BATTERY_FULL_RELEASE_MV < BATTERY_FULL_CONFIRM_MV &&
              BATTERY_FULL_CONFIRM_MV <= BATTERY_MAX_MV, "Invalid FULL hysteresis.");
static_assert(BATTERY_LOW_CONFIRM_MS > 0UL && BATTERY_FULL_CONFIRM_MS > 0UL &&
              BATTERY_RECOVER_STABLE_MS > 0UL, "Battery timers must be positive.");
static_assert(BATTERY_LOW_LOOP_DELAY_MS > 0UL && BATTERY_LOW_LOOP_DELAY_MS <= 1000UL,
              "Invalid low battery loop delay.");
static_assert(BATTERY_LOW_OLED_REFRESH_MS > 0UL && BATTERY_LOW_OLED_REFRESH_MS < 0x80000000UL,
              "Invalid low battery OLED refresh interval.");
static_assert(RPM_IR_TRIGGER_EDGE == CHANGE, "RPM must capture both edges and lock to the first edge polarity.");
static_assert(PULSES_PER_REV == 1UL, "Each same-polarity interval must equal one revolution.");
static_assert(RPM_ISR_QUEUE_SIZE >= 2U && RPM_ISR_QUEUE_SIZE <= 4096U,
              "RPM queue size must be between 2 and 4096.");
static_assert(RPM_VALID_MAX > 0UL && RPM_VALID_MAX <= 65535UL, "Invalid RPM limit.");
static_assert(POST_LAUNCH_FINISH_PERCENT > 0U && POST_LAUNCH_FINISH_PERCENT <= 100U,
              "Invalid finish percentage.");
static_assert(LOAD_IR_DEBOUNCE_US >= 10000UL && LOAD_IR_DEBOUNCE_US < 0x80000000UL,
              "LOAD state must remain unchanged for at least 10 ms.");
static_assert(OLED_MAX_HOLD_MS >= 2000UL && OLED_MAX_HOLD_MS < 0x80000000UL,
              "MAX display hold must be at least 2000 ms.");
static_assert(OLED_I2C_DATA_CHUNK_SIZE > 0U && OLED_I2C_DATA_CHUNK_SIZE <= 128U,
              "Invalid OLED I2C chunk size.");

/* [新增] 設定 API 結構參數檢查。 */
static_assert(BLE_OFF_WHILE_CHARGING <= 1U, "Charging BLE policy must be 0 or 1.");
static_assert(SETTINGS_COMMAND_QUEUE_SIZE > 0U && SETTINGS_COMMAND_QUEUE_SIZE <= 32U,
              "Invalid settings command queue size.");

#endif
