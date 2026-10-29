/*
    [PRO 修改] 由 BRD_BPX V1.2 衍生獨立 PRO 硬體定義，保留設定與 OTA 格式。
    檔案位置: BRD_BPX_PRO/brd_config.h
    [PRO 修改] 取代 OLED 硬體為 160x80 橫向 TFT；未改動測速核心、BLE、設定 API 與 OTA。
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

#define PROJECT_FULL_NAME                 "Beyblade RPM Detector BPX PRO"
#define PROJECT_SHORT_NAME                "BRD PRO"
/* [V1.2 修改] 韌體版本更新為 V1.2；[刪減] 原 V1.1 版本字串。 */
#define PROJECT_FIRMWARE_VERSION          "V1.4" /* [修改] RPM 優先，發射後才建構曲線 */
#define PROJECT_VERSION                   "BPX PRO " PROJECT_FIRMWARE_VERSION
#define PROJECT_BOOT_NAME                 "BRD BPX PRO"
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
#define BBP_NOTIFY_TIMEOUT_MS             1500UL
#define BBP_RETRY_INTERVAL_MS             1000UL
/*
    [BRD_BBP 修正 新增] 預設對應使用者確認可用的 V0.1 主動通知流程。
    1: 訂閱後、LOAD 改變送 A0，發射紀錄完成送 B0~B7 / 70~73。
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
static_assert(BBP_PUBLISH_MIN_DELAY_US < 2000000UL,
              "Publish must fit inside OLED hold interval.");
static_assert(BBP_BATTERY_CRITICAL_PERCENT <= BBP_BATTERY_WARNING_PERCENT &&
              BBP_BATTERY_WARNING_PERCENT <= 100U, "Invalid BBP battery thresholds.");

/* [V0.10 修改] RPM、LOAD 與 ADC 不啟用內部上下拉；充電 DET 例外使用上拉。 */
#define RPM_IR_GPIO                       3
#define RPM_IR_INPUT_MODE                 INPUT
/* [修改] RPM CHANGE 擷取兩種極性，正->正及負->負分別形成單圈週期。
   一組波形週期提供兩組時間錯開的單圈估算，非將半圈週期當一圈。 */
#define RPM_IR_TRIGGER_EDGE               CHANGE
#define LOAD_IR_GPIO                      1
#define LOAD_IR_INPUT_MODE                INPUT
/* [刪減] LOAD 改為 ADC，不再使用 CHANGE GPIO 中斷。 */
#define LOAD_ACTIVE_LEVEL                 HIGH /* 量測狀態機內部邏輯 ACTIVE=HIGH */
#define LOAD_ADC_RESOLUTION_BITS          12U
#define LOAD_ADC_ATTENUATION              ADC_ATTEN_DB_12
/* [修改] GPIO1 12-bit 原始 ADC 嚴格小於 4000 = 裝載。
 * [刪除] 先前 mV=1600/1950 遲滯判斷；100ms 穩定去抖保留。
 * [注意] 門檻接近滿刻度，若未裝載時 ADC <4000，會被判定為裝載。 */
#define LOAD_ADC_ACTIVE_RAW_THRESHOLD     4000U
static_assert(LOAD_ADC_ACTIVE_RAW_THRESHOLD <= 4095U, "LOAD ADC raw threshold invalid");
#define LOAD_ADC_SAMPLE_INTERVAL_MS       20UL
#define LOAD_ADC_STALE_TIMEOUT_MS         300UL
#define BATTERY_ADC_GPIO                  0
#define CHRG_DET_GPIO                     10
/* [V0.10 修正] 充電 DET 沿用原電路 LOW=充電、滿電高阻，恢復內部上拉。 */
#define CHRG_DET_INPUT_MODE               INPUT_PULLUP
#define CHRG_ACTIVE_LEVEL                 LOW
/* [PRO 刪減] GPIO8 不再用作 CHRG 狀態 LED，改為 TFT 背光開關。 */
#define BUTTON_GPIO                       5
#define BUTTON_INPUT_MODE                 INPUT_PULLUP
#define BUTTON_ACTIVE_LEVEL               LOW
/* [修改] UI 單鍵頁面切換、64 點顯示；BLE 各極性 32 點，格式不變。 */
#define UI_BUTTON_DEBOUNCE_MS               40UL
// [新增] 完整短按的最長持續時間，長按不會觸發導覽。
#define UI_BUTTON_SHORT_PRESS_MAX_MS        800UL
// [修改] Main/Max, Graph, History, IR TEST 共四個本地頁面。
#define UI_LOCAL_PAGE_COUNT                 4U
#define SHOT_RAW_SAMPLE_CAPACITY            8192U /* [新增] 8-byte/point，共 64KiB；30k RPM 約可記 8.2 秒雙邊緣 */
#define UI_CURVE_POINTS                     64U /* [修改] TFT 最多 64 個逐點顯示 */
#define SYS_POWER_GPIO                    6
/* [PRO 注意] 電源自鎖預設高有效，使用前應由電路圖確認。 */
#define SYS_POWER_ACTIVE_LEVEL            HIGH
#define IR_SWITCH_GPIO                    7
/* [PRO 注意] IR MOS 開關預設高有效，使用前應由電路圖確認。 */
#define IR_SWITCH_ACTIVE_LEVEL            HIGH
#define IR_RPM_ADC_GPIO                   4
/* [新增] IR TEST 右側 RPM ADC 條狀圖；只取原始 ADC 數值，不修改 GPIO3 中斷。 */
#define IR_RPM_ADC_RESOLUTION_BITS        12U
#define IR_RPM_ADC_ATTENUATION            ADC_ATTEN_DB_12
#define IR_RPM_ADC_SAMPLE_INTERVAL_MS     50UL
#define IR_RPM_ADC_STALE_TIMEOUT_MS       300UL
#define TFT_DC_GPIO                       2
#define TFT_BACKLIGHT_GPIO                8
#define TFT_BACKLIGHT_ON_LEVEL            HIGH
#define TFT_SPI_MOSI_GPIO                 20
#define TFT_SPI_SCK_GPIO                  21
/* [修改] ST7735S 實際面板原生 80x160，rotation=1 後顯示為 160x80。
   CS = GND，RESET 使用外部上電復位電路；SPI 腳位不變。 */
/* [修改] 2026-10-29：僅保留風格三 TORQUE，唯一有效索引為 0。 */
#define BRD_UI_MOTION_THEME 0U // [修改] 固定 TORQUE（原 06_TORQUE，舊索引 2）。
/* [保留] TORQUE 底部 16 段 RPM 指示條滿格基準（僅影響視覺比例）。
   20000 RPM = 100% 滿格；超過仍顯示滿格，數字保留真實 MAX RPM。
   此值不改變 RPM 偵測、結束門檻、BLE 資料與曲線自動比例。 */
#define BRD_UI_INDICATOR_FULL_SCALE_RPM   20000UL
static_assert(BRD_UI_INDICATOR_FULL_SCALE_RPM > 0UL,
              "UI indicator RPM full scale must be greater than zero.");
#define TFT_WIDTH                         160U
#define TFT_HEIGHT                        80U
#define TFT_ROTATION                      1U
/* [修改] SPI 現為 16 MHz；使用前仍需實機確認穩定性。 */
#define TFT_SPI_FREQUENCY_HZ              16000000UL

/* [修改] UI 動畫排程設定（毫秒）：統一 40 ms，目標 25 FPS。 */
/* [保留] 實際 FPS 受繪圖時間與 SPI 傳輸限制，40 ms 只是排程上限。 */
#define BRD_UI_MAIN_FRAME_INTERVAL_MS      40U
#define BRD_UI_GRAPH_FRAME_INTERVAL_MS     40U
#define BRD_UI_HISTORY_FRAME_INTERVAL_MS   40U
#define BRD_UI_IR_FRAME_INTERVAL_MS        40U
// [修改] 無閃電充電提示：只有電池內亮帶每 240 ms 更新相位。
// [保留] 只更新變化區域；不影響測速、BLE、電源管理。
#define BRD_UI_CHARGE_ANIM_INTERVAL_MS    240UL
#define BRD_UI_WARNING_FRAME_INTERVAL_MS   500U
// [修改] GPIO5 切換四頁時，每步 40 ms（5 步名義共 200 ms）。
#define BRD_UI_TRANSITION_STEP_INTERVAL_MS 40U
/* [新增] TFT 外層取樣/更新節流；必須 <= 動畫排程間隔才能達到目標 FPS。 */
#define BRD_TFT_POLL_INTERVAL_MS          10U
#define BRD_TFT_WARNING_POLL_INTERVAL_MS  100U
/* [刪除] TFT_TRANSFER_ROWS：直接使用 Adafruit GFX 繪製，無自製分段像素傳輸。 */
/* [保留] Adafruit 80x160 plugin GRAM 偏移由庫自動處理。 */
/* [修改] 一般 0.96 吋 80x160 breakout 先試用 MINI profile。
   MINI colstart=24,rowstart=0；PLUGIN colstart=26,rowstart=1。
   若邊框偏移或顏色異常，改回 INITR_MINI160x80_PLUGIN，
   並同步將 TFT_INVERSION_ON 設為 1U。 */
#define TFT_ADAFRUIT_INIT_PROFILE         INITR_MINI160x80
/* [修改] MINI 需使用普通反相命令，預設 false（0U）。
   舊 PLUGIN profile 的反相命令意義相反，原本使用 1U。 */
#define TFT_INVERSION_ON                  0U
/* [新增] 面板色序校正：1=BGR(紅藍通道交換)；0=RGB(原官方 MINI 設定)。
   只改 ST7735 MADCTL 的 RGB/BGR 位元，不更動畫面座標或顏色常數。
   若實機紅色顯示紅、藍色顯示藍，請設為 0U。 */
#ifndef TFT_PANEL_BGR_ORDER
#define TFT_PANEL_BGR_ORDER                 1U
#endif
/* [新增] 上電後等待面板電源、SPI 線路穩定，再執行兩次 SWRESET。
   GPIO21 = ESP32-C3 UART0 TX，ROM 開機可能輸出 UART 波形，
   CS 接地無法屏蔽該階段。RESET RC 硬體仍是必要建議。 */
#define TFT_POWER_STABLE_WAIT_MS          180UL
#define TFT_REINIT_GAP_MS                 80UL

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
#define BATTERY_LOW_STOP_PERCENT          5U                                            // 低電保護百分比 5%
#define BATTERY_BOOT_MIN_PERCENT          8U                                            // 開機最小電量百分比 8%
#define BATTERY_RECOVER_PERCENT           10U                                           // 低電保護解除百分比 10%
#define BATTERY_LOW_LOOP_DELAY_MS         20UL
#define BATTERY_LOW_TFT_REFRESH_MS        1000UL

/*
    [V0.10 新增] LOAD 去抖與 MAX 自鎖時間集中在同一區。
    去抖: LOAD ADC 經雙門檻分類後，邏輯狀態維持的時間，單位 us。
    自鎖: MAX 完整畫面成功送出後至少保持的時間，單位 ms。
    OLED 故障時仍以量測完成時間作為解鎖備援，避免永久卡住。
    解鎖後重新比對 LOAD ADC 分類結果並重新去抖。
*/
/* [修改] LOAD ADC 的狀態變化維持 100 ms 才確認，不再建立 LOAD ISR Queue。 */
#define LOAD_IR_DEBOUNCE_US             100000UL

/*
    [新增] 量測模式選擇。
    BRD_MEASUREMENT_MODE_LOAD:
        GPIO1 LOAD ADC 分類為 ACTIVE 並維持 100 ms 後，清零啟動 RPM；量測後 LOAD 不參與發射判定。
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
#define RPM_ISR_QUEUE_SIZE               256U /* [修改 V1.4] 提高高轉速期間 TFT/BLE 短暫佔用時的 ISR 緩衝餘裕 */
#define RPM_MIN_PERIOD_US                500UL
#define RPM_MAX_PERIOD_US                1000000UL
#define RPM_VALID_MAX                    60000UL
#define RPM_ZERO_TIMEOUT_MS              300UL
#define PRELAUNCH_IDLE_RESET_MS           3000UL
#define POST_LAUNCH_NO_RPM_TIMEOUT_MS      1200UL
/* [修改] 兩模式共用預設 30%；BLE API 可以修改為 1..100%。 */
#define POST_LAUNCH_FINISH_PERCENT        30U

/* [PRO 刪減] 原 SSD1306 I2C 0x3C、128x32 OLED 相關參數。 */
#define TFT_UPDATE_INTERVAL_MS            100UL
#define TFT_BOOT_VERSION_DISPLAY_MS      1500UL
#define TFT_MAX_HOLD_MS                   2500UL
#define TFT_LOW_REFRESH_MS               1000UL

/* [V0.12 新增] ADC、事件佇列及計時參數的編譯期檢查。 */
static_assert(BATTERY_ADC_RESOLUTION_BITS == 12U, "ESP32-C3 ADC requires 12-bit configuration.");
static_assert(BATTERY_ADC_STALE_TIMEOUT_MS > BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_ADC_STALE_TIMEOUT_MS < 0x80000000UL, "Invalid ADC stale timeout.");
static_assert(BATTERY_RECOVER_STABLE_MS > 0UL && BATTERY_RECOVER_STABLE_MS < 0x80000000UL,
              "Invalid battery recovery duration.");
static_assert(BATTERY_SAMPLE_INTERVAL_MS < 0x80000000UL / 3UL &&
              BATTERY_RECOVER_MAX_GAP_MS >= BATTERY_SAMPLE_INTERVAL_MS &&
              BATTERY_RECOVER_MAX_GAP_MS < BATTERY_ADC_STALE_TIMEOUT_MS, "Invalid recovery sample gap.");
static_assert(IR_RPM_ADC_RESOLUTION_BITS == 12U, "RPM ADC must be 12-bit.");
static_assert(IR_RPM_ADC_SAMPLE_INTERVAL_MS > 0U &&
              IR_RPM_ADC_SAMPLE_INTERVAL_MS < IR_RPM_ADC_STALE_TIMEOUT_MS,
              "Invalid RPM ADC sample period.");
static_assert(LOAD_ADC_RESOLUTION_BITS == 12U, "LOAD ADC must be 12-bit.");
static_assert(LOAD_ADC_ACTIVE_RAW_THRESHOLD > 0U && LOAD_ADC_ACTIVE_RAW_THRESHOLD <= 4095U,
              "Invalid LOAD ADC raw threshold.");
static_assert(LOAD_ADC_SAMPLE_INTERVAL_MS > 0 &&
              LOAD_ADC_SAMPLE_INTERVAL_MS < LOAD_ADC_STALE_TIMEOUT_MS,
              "LOAD ADC sample / stale time configuration invalid.");
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
static_assert(BATTERY_LOW_STOP_PERCENT > 0U &&
              BATTERY_LOW_STOP_PERCENT < BATTERY_RECOVER_PERCENT &&
              BATTERY_RECOVER_PERCENT <= 100U,
              "Runtime battery thresholds must satisfy 0 < stop < recover <= 100.");
static_assert(BATTERY_BOOT_MIN_PERCENT > BATTERY_LOW_STOP_PERCENT &&
              BATTERY_BOOT_MIN_PERCENT < 100U,
              "Boot battery threshold must satisfy stop < boot < 100.");
static_assert(BATTERY_LOW_LOOP_DELAY_MS > 0UL && BATTERY_LOW_LOOP_DELAY_MS <= 1000UL,
              "Invalid low battery loop delay.");
static_assert(BATTERY_LOW_TFT_REFRESH_MS > 0UL && BATTERY_LOW_TFT_REFRESH_MS < 0x80000000UL,
              "Invalid low battery TFT refresh interval.");
static_assert(RPM_IR_TRIGGER_EDGE == CHANGE, "RPM must capture both edges and lock to the first edge polarity.");
static_assert(PULSES_PER_REV == 1UL, "Each same-polarity interval must equal one revolution.");
static_assert(SHOT_RAW_SAMPLE_CAPACITY >= UI_CURVE_POINTS && SHOT_RAW_SAMPLE_CAPACITY <= 32767U,
              "Raw shot buffer must fit index and maximum shot duration constraints.");
static_assert(RPM_ISR_QUEUE_SIZE >= 2U && RPM_ISR_QUEUE_SIZE <= 256U,
              "RPM queue size must be between 2 and 256.");
static_assert(RPM_VALID_MAX > 0UL && RPM_VALID_MAX <= 65535UL, "Invalid RPM limit.");
static_assert(POST_LAUNCH_FINISH_PERCENT > 0U && POST_LAUNCH_FINISH_PERCENT <= 100U,
              "Invalid finish percentage.");
static_assert(LOAD_IR_DEBOUNCE_US >= 100000UL && LOAD_IR_DEBOUNCE_US < 0x80000000UL,
              "LOAD ADC state must remain unchanged for at least 100 ms.");
static_assert(TFT_MAX_HOLD_MS >= 2000UL && TFT_MAX_HOLD_MS < 0x80000000UL,
              "MAX display hold must be at least 2000 ms.");
/* [PRO 新增] TFT 配置的編譯期檢查。 */
static_assert(TFT_WIDTH == 160U && TFT_HEIGHT == 80U, "Unexpected TFT landscape geometry.");
static_assert(TFT_ROTATION == 1U, "Unexpected TFT orientation.");
static_assert(TFT_POWER_STABLE_WAIT_MS >= 120UL && TFT_REINIT_GAP_MS >= 40UL,
              "ST7735 startup stabilization delay is too short.");
static_assert(TFT_INVERSION_ON <= 1U, "Invalid TFT inversion setting.");
static_assert(TFT_SPI_FREQUENCY_HZ <= 40000000UL, "SPI frequency exceeds driver recommendation.");
static_assert(TFT_DC_GPIO != RPM_IR_GPIO && TFT_BACKLIGHT_GPIO != LOAD_IR_GPIO, "TFT GPIO conflict.");

/* [新增] 設定 API 結構參數檢查。 */
static_assert(BLE_OFF_WHILE_CHARGING <= 1U, "Charging BLE policy must be 0 or 1.");
static_assert(SETTINGS_COMMAND_QUEUE_SIZE > 0U && SETTINGS_COMMAND_QUEUE_SIZE <= 32U,
              "Invalid settings command queue size.");

/* [刪除] 手寫 framebuffer 分段列數的編譯期檢查。 */

#endif
