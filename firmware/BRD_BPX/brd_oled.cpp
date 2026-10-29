/*
    檔案位置: BRD_BPX/brd_oled.cpp
    [修改] 充電導致 BLE 停用時，右下角分兩行顯示 BLE / OFF 小字。
    [刪減] 原本帶斜線的藍牙停用圖示。
    [修改] BLE 停用狀態改變時立即安排下一幅畫面，不等待固定更新週期。
    [V1.2 修改] OTA 下方進度由 "數值 PCT" 改為左側扇形圓盤與中央百分比。
    [V1.2 新增] 0% 保留圓形外框、1~99% 由 12 點方向順時針填滿、100% 完全實心。
    [V1.2 新增] % 字型；OTA 數值不使用 PCT 字串，版本號統一由 brd_config.h 設定。
    [修正] OTA 大字百分比以 10x14 專用空心圓圈字型繪製，不再顯示成實心點。
    [V0.10 修改] 顯示 LOAD 狀態、RPM / MAX 與開機版本。
    [V1.4 OLED UI 修改] 主頁 LOAD 狀態文字為 WAIT LOAD / LOADED；即時數值 RPM -> SP。
    [V1.4 OLED UI 保留] MAX 標籤與 SP 一樣使用原版 2 倍字體。
    [V1.4 OLED UI 刪減] 移除 SP 後新增的額外空白，回復 "SP 15800"。
    [V1.4 OLED 介面修改] 右上角僅顯示閃電 x96..100 與電池 x105..124，移除 SOC 數字及 %。
    [V1.4 OLED UI 刪除] 充電黑色掃描線動畫與動畫計時常數，電池圖示固定依 SOC 填充。
    [V1.4 OLED UI 刪除] 主頁不顯示 FULL 字樣，滿電時保留靜態電池並熄滅閃電。
    [V0.10 刪減] OLED 休眠與獨立 FreeRTOS Task。
    [V0.10 修改] 不使用 Wire.begin，避免框架初始化自動開啟內部上拉。
    [V0.10 新增] I2C 明確不開內部上拉；初始化或傳送失敗每隔一秒重試。
    [V0.10 新增] 一般畫面分段寫入，每次 loop 最多一個 I2C 傳送。
    [V0.10 修改] 低電優先顯示圓角電池、左側短條與中央閃電圖示。
    [V0.10 新增] MAX 自鎖期間第一行顯示 HOLD，自鎖結束恢復裝載狀態。
    [V0.12 新增] ADC ERR 專用畫面與 C 字型，故障恢復時重畫一般狀態。
    狀態與畫面皆在主 loop 存取，不再有跨 Task 同時讀寫量測資料的問題。
*/
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <cstring>
#include <cstdio>

#include "brd_battery.h"
#include "brd_bbp.h"
#include "brd_config.h"
/* [新增] OLED 狀態文字跟隨執行時模式。 */
#include "brd_settings.h"
/* [新增] 讀取目前 BLE 控制的 OLED 顯示頁。 */
#include "brd_settings_ble.h"
/* [V1.2 新增] OTA 唯讀畫面資料。 */
#include "brd_ota.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_oled.h"

static i2c_master_bus_handle_t g_oled_bus = nullptr;
static i2c_master_dev_handle_t g_oled_device = nullptr;
static uint8_t g_oled_buffer[(OLED_WIDTH * OLED_HEIGHT) / 8U];
static bool g_oled_available = false;
static bool g_frame_active = false;
static bool g_page_address_pending = true;
static uint8_t g_page = 0U;
static uint16_t g_column = 0U;
static uint32_t g_last_frame_start_ms = 0UL;
static uint32_t g_last_retry_ms = 0UL;
static brd_display_t g_frame_display = {};
/* [V0.10 恢復] 每幅畫面保存一致的電量與充電狀態。 */
static uint8_t g_frame_battery_percent = 0U;
static bool g_frame_charging = false;
/* [V1.4 OLED 刪除] 黑線動畫時序已移除；充電時不再額外更新掃描相位。 */
/* [V1.4 OLED 修改] 右上角圖示靠右，保留 4px 間距；不保留文字百分比。 */
static constexpr uint8_t OLED_MAIN_BATTERY_X = 105U;
static constexpr uint8_t OLED_MAIN_LIGHTNING_X = 96U;
/* [V1.4 OLED UI 保留] FULL 變化仍觸發重繪，用於滿電時熄滅閃電；不顯示 FULL 文字。 */
static bool g_frame_full = false;
/* [BRD_BBP 新增] 連線狀態變化立即安排畫面更新。 */
static bool g_frame_ble_connected = false;
/* [新增] 保存本幅畫面的充電停用狀態，與單純未連線分開判斷。 */
static bool g_frame_ble_disabled_for_charging = false;
static bool g_frame_is_low_battery = false;
static brd_ota_display_t g_frame_ota = {};
/* [修改] IR TEST 右上角顯示無 BAT 前綴的 x.xxV 電壓。 */
static uint8_t g_frame_display_page = BRD_DISPLAY_PAGE_MAIN;
static bool g_frame_ir1_triggered = false;
static bool g_frame_ir2_triggered = false;
static uint16_t g_frame_voltage_mv = 0U;
/* [V0.12 新增] ADC 故障有獨立畫面，不冒充 0% 低電。 */
static bool g_frame_is_adc_fault = false;
static bool g_warning_frame_drawn = false;
static uint32_t g_last_warning_frame_ms = 0UL;

static void oled_mark_failed(void) {
    g_oled_available = false;
    g_frame_active = false;
    g_warning_frame_drawn = false;
    g_last_retry_ms = millis();
}

static bool oled_transmit(const uint8_t *data, size_t length) {
    if (g_oled_device == nullptr || data == nullptr || length == 0U) {
        return false;
    }
    return i2c_master_transmit(g_oled_device, data, length, OLED_I2C_TIMEOUT_MS) == ESP_OK;
}

static bool oled_initialize(void) {
    /* [V0.10 新增] 每次初始化及復原都先確保 SDA/SCL 不存在內部上下拉。 */
    (void)gpio_set_pull_mode((gpio_num_t)I2C_SDA_GPIO, GPIO_FLOATING);
    (void)gpio_set_pull_mode((gpio_num_t)I2C_SCL_GPIO, GPIO_FLOATING);

    if (g_oled_bus == nullptr) {
        i2c_master_bus_config_t config = {};
        config.i2c_port = I2C_NUM_0;
        config.sda_io_num = (gpio_num_t)I2C_SDA_GPIO;
        config.scl_io_num = (gpio_num_t)I2C_SCL_GPIO;
        config.clk_source = I2C_CLK_SRC_DEFAULT;
        config.glitch_ignore_cnt = 7U;
        config.flags.enable_internal_pullup = false;

        if (i2c_new_master_bus(&config, &g_oled_bus) != ESP_OK) {
            return false;
        }
    } else {
        /* [V0.10 新增] 重試時復原 I2C 控制器，不重啟 MCU、不重設量測結果。 */
        if (i2c_master_bus_reset(g_oled_bus) != ESP_OK) {
            return false;
        }
    }

    if (g_oled_device == nullptr) {
        i2c_device_config_t device = {};
        device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device.device_address = OLED_I2C_ADDRESS;
        device.scl_speed_hz = OLED_I2C_FREQUENCY_HZ;

        if (i2c_master_bus_add_device(g_oled_bus, &device, &g_oled_device) != ESP_OK) {
            return false;
        }
    }

    if (i2c_master_probe(g_oled_bus, OLED_I2C_ADDRESS, OLED_I2C_TIMEOUT_MS) != ESP_OK) {
        return false;
    }

    /* [保留] SSD1306 指令與 V1.9 顯示方向；第一個 byte 為 command control byte。 */
    static const uint8_t commands[] = {
        0x00U,
        0xAEU,
        0xD5U, 0x80U,
        0xA8U, 0x1FU,
        0xD3U, 0x00U,
        0x40U,
        0x8DU, 0x14U,
        0x20U, 0x02U,
        0xA0U,
        0xC0U,
        0xDAU, 0x02U,
        0x81U, 0x8FU,
        0xD9U, 0xF1U,
        0xDBU, 0x40U,
        0xA4U,
        0xA6U,
        0x2EU,
        0xAFU
    };
    return oled_transmit(commands, sizeof(commands));
}

/* [保留] 5x7 字型與像素繪製沿用附件 V1.9，包含 B、X、V 與小數點。 */
static void oled_clear_buffer(void) {
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
}

static void oled_set_pixel(uint8_t x, uint8_t y, bool on) {
    uint16_t index;
    uint8_t mask;

    if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return;
    }

    index = (uint16_t)x + ((uint16_t)(y / 8U) * OLED_WIDTH);
    mask = (uint8_t)(1U << (y & 0x07U));

    if (on == true) {
        g_oled_buffer[index] |= mask;
    } else {
        g_oled_buffer[index] &= (uint8_t)(~mask);
    }
}

static const uint8_t *oled_get_glyph(char c) {
    static const uint8_t glyph_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t glyph_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t glyph_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
    static const uint8_t glyph_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t glyph_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t glyph_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t glyph_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t glyph_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
    static const uint8_t glyph_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t glyph_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t glyph_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
    static const uint8_t glyph_A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    /* [保留] 開機 BRD 標題所需 B 字元。 */
    static const uint8_t glyph_B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
    /* [V0.12 新增] ADC ERR 所需的 C 字型。 */
    static const uint8_t glyph_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
    static const uint8_t glyph_D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t glyph_E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
    /* [V0.10 新增] HOLD 所需 H 字元，其餘 O、L、D 沿用既有字庫。 */
    static const uint8_t glyph_H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
    static const uint8_t glyph_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
    static const uint8_t glyph_L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t glyph_M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    /* [V1.2 新增] OTA DONE 的 N，避免完成標籤出現缺字。 */
    static const uint8_t glyph_N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
    static const uint8_t glyph_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
    static const uint8_t glyph_P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t glyph_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    /* [新增] IR TEST 頁面標題需要 S，原 OLED 5x7 字庫未包含。 */
    static const uint8_t glyph_S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t glyph_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    /* [新增] AUTO READY 顯示所需 U 字元。 */
    static const uint8_t glyph_U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
    /* [保留] PROJECT_VERSION 顯示所需 V 字元。 */
    static const uint8_t glyph_V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
    static const uint8_t glyph_W[5] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
    /* [保留] MAX 標籤所需 X 字元。 */
    static const uint8_t glyph_X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t glyph_Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
    /* [保留] PROJECT_VERSION 小數點。 */
    static const uint8_t glyph_dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
    /* [V1.2 新增] OTA 進度百分比字型，取代原有 PCT 單位。 */
    static const uint8_t glyph_percent[5] = {0x63, 0x13, 0x08, 0x64, 0x63};
    /* [BRD_BBP 新增] 版本名稱使用的底線字元。 */
    static const uint8_t glyph_underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40};

    /* [新增] FW VER 開機版本資訊使用的 F 字元，BPX 的 X 沿用既有字庫。 */
    static const uint8_t glyph_F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
    switch (c) {
        case '0': return glyph_0;
        case '1': return glyph_1;
        case '2': return glyph_2;
        case '3': return glyph_3;
        case '4': return glyph_4;
        case '5': return glyph_5;
        case '6': return glyph_6;
        case '7': return glyph_7;
        case '8': return glyph_8;
        case '9': return glyph_9;
        case 'A': return glyph_A;
        case 'B': return glyph_B;
        case 'C': return glyph_C;
        case 'D': return glyph_D;
        case 'E': return glyph_E;
        case 'F': return glyph_F;
        case 'H': return glyph_H;
        case 'I': return glyph_I;
        case 'L': return glyph_L;
        case 'M': return glyph_M;
        case 'N': return glyph_N;
        case 'O': return glyph_O;
        case 'P': return glyph_P;
        case 'R': return glyph_R;
        case 'S': return glyph_S;
        case 'T': return glyph_T;
        case 'U': return glyph_U;
        case 'V': return glyph_V;
        case 'W': return glyph_W;
        case 'X': return glyph_X;
        case 'Y': return glyph_Y;
        case '.': return glyph_dot;
        case '%': return glyph_percent;
        case '_': return glyph_underscore;
        case ' ':
        default:
            return glyph_space;
    }
}

static void oled_draw_char(uint8_t x, uint8_t y, char c, uint8_t scale) {
    const uint8_t *glyph;
    uint8_t column;
    uint8_t row;
    uint8_t sx;
    uint8_t sy;

    if (scale == 0U) {
        return;
    }

    /*
        [修正] OTA 的 2 倍百分比字元使用獨立 10x14 點陣。
        [刪減] 避免原 5x7 字型放大後，上下兩個圓圈變成實心方塊（看起來像 ./.)。
        [新增] 上方 x0..3/y0..3、下方 x6..9/y10..13 皆為空心封閉圓圈，
               中間以單像素斜線連結視覺方向；維持原字元 12px 間距。
    */
    if (c == '%' && scale == 2U) {
        static const uint16_t percent_rows[14] = {
            0x206U, 0x209U, 0x109U, 0x106U,
            0x080U, 0x080U, 0x040U, 0x020U,
            0x020U, 0x010U, 0x188U, 0x248U,
            0x244U, 0x184U
        };
        for (uint8_t row = 0U; row < 14U; row++) {
            for (uint8_t column = 0U; column < 10U; column++) {
                if ((percent_rows[row] & (1U << column)) != 0U) {
                    oled_set_pixel((uint8_t)(x + column), (uint8_t)(y + row), true);
                }
            }
        }
        return;
    }

    glyph = oled_get_glyph(c);

    for (column = 0U; column < 5U; column++) {
        for (row = 0U; row < 7U; row++) {
            if ((glyph[column] & (1U << row)) == 0U) {
                continue;
            }

            for (sx = 0U; sx < scale; sx++) {
                for (sy = 0U; sy < scale; sy++) {
                    oled_set_pixel((uint8_t)(x + (column * scale) + sx),
                                   (uint8_t)(y + (row * scale) + sy),
                                   true);
                }
            }
        }
    }
}

static void oled_draw_text(uint8_t x, uint8_t y, const char *text, uint8_t scale) {
    uint8_t cursor_x;
    uint8_t char_width;

    if ((text == nullptr) || (scale == 0U)) {
        return;
    }

    cursor_x = x;
    char_width = (uint8_t)(6U * scale);

    while (*text != '\0') {
        if (((uint16_t)cursor_x + (5U * scale)) > OLED_WIDTH) {
            break;
        }

        oled_draw_char(cursor_x, y, *text, scale);
        cursor_x = (uint8_t)(cursor_x + char_width);
        text++;
    }
}

static void oled_draw_centered_text(uint8_t y, const char *text, uint8_t scale) {
    size_t text_length;
    uint16_t text_width;
    uint8_t start_x;

    if ((text == nullptr) || (scale == 0U)) {
        return;
    }

    text_length = strlen(text);

    if (text_length == 0U) {
        return;
    }

    text_width = (uint16_t)(text_length * 6U * scale);

    /* 最後一個字元不需要右側字距。 */
    text_width -= scale;

    if (text_width >= OLED_WIDTH) {
        start_x = 0U;
    } else {
        start_x = (uint8_t)((OLED_WIDTH - text_width) / 2U);
    }

    oled_draw_text(start_x, y, text, scale);
}

/*
    [新增] IR TEST 電壓文字右對齊輔助函式。
    位置固定在螢幕最右側；只使用既有 OLED 字庫與 framebuffer。
*/
static void oled_draw_right_aligned_text(uint8_t right_x, uint8_t y, const char *text, uint8_t scale) {
    if (text == nullptr || scale == 0U) {
        return;
    }
    size_t length = strlen(text);
    if (length == 0U) {
        return;
    }
    uint16_t width = static_cast<uint16_t>(length * 6U * scale - scale);
    uint8_t x = (width > static_cast<uint16_t>(right_x) + 1U) ?
        0U : static_cast<uint8_t>(static_cast<uint16_t>(right_x) + 1U - width);
    oled_draw_text(x, y, text, scale);
}

/*
    [新增] 電池電壓由 mV 換算為 0.01V，四捨五入，不顯示 BAT 前綴。
    例如 4124mV -> 4.12V、4125mV -> 4.13V。
*/
static void oled_format_ir_voltage(uint16_t voltage_mv, char *out, size_t size) {
    if (out == nullptr || size == 0U) {
        return;
    }
    uint32_t centivolts = (static_cast<uint32_t>(voltage_mv) + 5U) / 10U;
    snprintf(out, size, "%lu.%02luV",
             static_cast<unsigned long>(centivolts / 100U),
             static_cast<unsigned long>(centivolts % 100U));
}

/*
    [V1.2 新增] OTA 扇形圓盤：左側 17x17 像素，0% 為圓框，100% 為實心圓。
    [V1.2 修改] 扇形從 12 點方向順時針增加，填滿圓內部而非僅旋轉弧線。
    不使用 atan2/sin/cos 浮點運算或動態記憶體，避免 OTA 傳輸期間增加 CPU 負擔。
    每象限使用整數斜率比值近似角度，128x32 OLED 的像素精度足夠。
*/
static void oled_draw_ota_sector(uint8_t cx, uint8_t cy, uint8_t radius, uint8_t percent) {
    if (percent > 100U) {
        percent = 100U;
    }

    const int16_t r = static_cast<int16_t>(radius);
    const int16_t outer_r2 = r * r;
    const int16_t inner_r2 = (r - 1) * (r - 1);

    for (int16_t dy = -r; dy <= r; dy++) {
        for (int16_t dx = -r; dx <= r; dx++) {
            const int16_t distance2 = dx * dx + dy * dy;
            if (distance2 > outer_r2) {
                continue;
            }

            /* [V1.2 新增] 外框始終保留，讓 0% 時也能看見完整圓盤。 */
            bool filled = (distance2 >= inner_r2);
            if (percent == 100U || (percent != 0U && dx == 0 && dy == 0)) {
                filled = true;
            } else if (percent != 0U) {
                uint8_t quarter;
                int16_t numerator;
                int16_t denominator;

                if (dx >= 0 && dy < 0) {
                    quarter = 0U;
                    numerator = dx;
                    denominator = dx - dy;
                } else if (dx > 0 && dy >= 0) {
                    quarter = 1U;
                    numerator = dy;
                    denominator = dx + dy;
                } else if (dx <= 0 && dy > 0) {
                    quarter = 2U;
                    numerator = -dx;
                    denominator = -dx + dy;
                } else {
                    quarter = 3U;
                    numerator = -dy;
                    denominator = -dx - dy;
                }

                /* [V1.2 新增] 0/25/50/75/100% 分別對應正上/右/下/左/滿圓。 */
                if (static_cast<uint32_t>(percent) * static_cast<uint32_t>(denominator) >=
                    static_cast<uint32_t>(quarter) * 25U * static_cast<uint32_t>(denominator) +
                    25U * static_cast<uint32_t>(numerator)) {
                    filled = true;
                }
            }

            if (filled) {
                oled_set_pixel(static_cast<uint8_t>(static_cast<int16_t>(cx) + dx),
                               static_cast<uint8_t>(static_cast<int16_t>(cy) + dy), true);
            }
        }
    }
}

/* [V0.10 恢復] 原版 17x9 電池本體、3x5 端子，內部填充寬度為 13 像素。 */
static void oled_draw_battery_icon(uint8_t x, uint8_t y, uint8_t percent) {
    const uint8_t body_width = 17U;
    const uint8_t body_height = 9U;
    const uint8_t inner_width = 13U;
    const uint8_t inner_height = 5U;

    if (percent > 100U) {
        percent = 100U;
    }
    for (uint8_t ix = 0U; ix < body_width; ix++) {
        oled_set_pixel((uint8_t)(x + ix), y, true);
        oled_set_pixel((uint8_t)(x + ix), (uint8_t)(y + body_height - 1U), true);
    }
    for (uint8_t iy = 0U; iy < body_height; iy++) {
        oled_set_pixel(x, (uint8_t)(y + iy), true);
        oled_set_pixel((uint8_t)(x + body_width - 1U), (uint8_t)(y + iy), true);
    }
    for (uint8_t ix = 0U; ix < 3U; ix++) {
        for (uint8_t iy = 0U; iy < 5U; iy++) {
            oled_set_pixel((uint8_t)(x + body_width + ix), (uint8_t)(y + 2U + iy), true);
        }
    }

    uint8_t fill_width = (uint8_t)((((uint16_t)percent * inner_width) + 50U) / 100U);
    for (uint8_t ix = 0U; ix < fill_width; ix++) {
        for (uint8_t iy = 0U; iy < inner_height; iy++) {
            oled_set_pixel((uint8_t)(x + 2U + ix), (uint8_t)(y + 2U + iy), true);
        }
    }
}

/* [V1.4 OLED 刪除] 移除 oled_draw_battery_charge_scan()：充電不再掃描黑線。
   [保留] oled_draw_battery_icon() 會直接依實際 SOC 繪製靜態填充。 */

/* [V0.10 恢復] 原版 5x7 充電閃電圖示，未充電時不繪製。 */
static void oled_draw_charging_icon(uint8_t x, uint8_t y) {
    static const uint8_t lightning_rows[7] = {
        0x04U, 0x0CU, 0x08U, 0x1EU, 0x06U, 0x04U, 0x08U
    };
    for (uint8_t row = 0U; row < 7U; row++) {
        for (uint8_t column = 0U; column < 5U; column++) {
            if ((lightning_rows[row] & (1U << (4U - column))) != 0U) {
                oled_set_pixel((uint8_t)(x + column), (uint8_t)(y + row), true);
            }
        }
    }
}

static void oled_begin_low_battery_frame(void) {
    /*
        [V0.10 刪減] 原本的方角空電池與中央驚嘆號。
        [V0.10 新增] 依參考圖繪製 60x28 圓角電池、左側低電量短條與中央閃電。
        本體加右側端子共 64x28，置中於 128x32 OLED。
        OLED 為單色，參考圖的紅色短條改用亮色像素表示。
        中央閃電屬於低電警示圖樣，不以 CHRG_DET 作為顯示條件。
    */
    static const uint8_t outer_corner_inset[4] = {4U, 2U, 1U, 0U};
    static const uint8_t inner_corner_inset[2] = {2U, 1U};
    static const uint16_t lightning_rows[18] = {
        0x004U, 0x00CU, 0x00CU, 0x018U, 0x038U, 0x038U,
        0x07FU, 0x07EU, 0x0FCU, 0x1FCU, 0x038U, 0x030U,
        0x030U, 0x060U, 0x060U, 0x040U, 0x040U, 0x040U
    };
    const uint8_t body_x = 32U;
    const uint8_t body_y = 2U;
    const uint8_t body_width = 60U;
    const uint8_t body_height = 28U;

    oled_clear_buffer();

    /* [新增] 兩像素圓角外框；逐列保留內部黑色區域。 */
    for (uint8_t row = 0U; row < body_height; row++) {
        uint8_t edge = row < body_height / 2U ? row : (uint8_t)(body_height - 1U - row);
        uint8_t outer_inset = edge < 4U ? outer_corner_inset[edge] : 0U;
        uint8_t inner_inset = body_width / 2U;

        if (edge >= 2U) {
            uint8_t inner_edge = (uint8_t)(edge - 2U);
            inner_inset = (uint8_t)(2U + (inner_edge < 2U ? inner_corner_inset[inner_edge] : 0U));
        }

        for (uint8_t column = outer_inset; column < body_width - outer_inset; column++) {
            if (column < inner_inset || column >= body_width - inner_inset) {
                oled_set_pixel((uint8_t)(body_x + column), (uint8_t)(body_y + row), true);
            }
        }
    }

    /* [新增] 右側 4x10 電池端子，外側兩角各留一個黑色像素。 */
    for (uint8_t row = 0U; row < 10U; row++) {
        for (uint8_t column = 0U; column < 4U; column++) {
            if (column != 3U || (row != 0U && row != 9U)) {
                oled_set_pixel((uint8_t)(body_x + body_width + column), (uint8_t)(11U + row), true);
            }
        }
    }

    /* [新增] 左側 5x20 短條，對應參考圖的紅色低電量區域。 */
    for (uint8_t x = 37U; x < 42U; x++) {
        for (uint8_t y = 6U; y < 26U; y++) {
            oled_set_pixel(x, y, true);
        }
    }

    /* [新增] 中央 9x18 閃電，以固定點陣維持單色小尺寸下的可辨識度。 */
    for (uint8_t row = 0U; row < 18U; row++) {
        for (uint8_t column = 0U; column < 9U; column++) {
            if ((lightning_rows[row] & (1U << (8U - column))) != 0U) {
                oled_set_pixel((uint8_t)(58U + column), (uint8_t)(7U + row), true);
            }
        }
    }

    g_frame_display = {};
    g_frame_is_low_battery = true;
    g_frame_is_adc_fault = false;
    g_page = 0U;
    g_column = 0U;
    g_page_address_pending = true;
    g_frame_active = true;
}

/* [V0.12 新增] ADC 從未有效或資料逾時，顯示專用診斷狀態。 */
static void oled_begin_adc_fault_frame(void) {
    oled_clear_buffer();
    oled_draw_centered_text(9U, "ADC ERR", 2U);
    g_frame_display = {};
    g_frame_is_low_battery = false;
    g_frame_is_adc_fault = true;
    g_page = 0U;
    g_column = 0U;
    g_page_address_pending = true;
    g_frame_active = true;
}

/* [R4 新增] 使用 BRD_BLE_OLED(3).zip 的原生 7x13 藍牙點陣。
   點陣及位置沿用附件：x119..125、y17..29，保留五位數 RPM/MAX 間距。 */
static void oled_draw_bluetooth_icon(void) {
    static const uint8_t rows[13] = {
        0x08U, 0x0CU, 0x0AU, 0x49U, 0x2AU, 0x1CU, 0x08U,
        0x1CU, 0x2AU, 0x49U, 0x0AU, 0x0CU, 0x08U
    };
    for (uint8_t row = 0U; row < 13U; row++) {
        for (uint8_t column = 0U; column < 7U; column++) {
            if ((rows[row] & (1U << (6U - column))) != 0U) {
                oled_set_pixel((uint8_t)(119U + column), (uint8_t)(17U + row), true);
            }
        }
    }
}

/* [修改] 使用既有 5x7 字型、1 倍大小，右下角分兩行顯示 BLE / OFF。
   [刪減] 斜線藍牙點陣；文字範圍 x111..127、y16..30，不占用五位數值區。 */
static void oled_draw_bluetooth_disabled_text(void) {
    const uint8_t x = (uint8_t)(OLED_WIDTH - 17U);
    oled_draw_text(x, 16U, "BLE", 1U);
    oled_draw_text(x, 24U, "OFF", 1U);
}

/* [新增] BLE 確實停止後才提示；設定保存的 200 ms 回覆期間不提前顯示停用。 */
static bool oled_bluetooth_disabled_for_charging(bool charging) {
    return charging && brd_settings_get().charging_ble_off != 0U && !brd_bbp_is_started();
}

/* [新增] 六邊形及梯形輪廓：Bresenham 逐點畫線，適用 128x32 OLED。 */
static void oled_draw_segment(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int step_x = x0 < x1 ? 1 : -1;
    int step_y = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < (int)OLED_WIDTH && y0 >= 0 && y0 < (int)OLED_HEIGHT) {
            oled_set_pixel((uint8_t)x0, (uint8_t)y0, true);
        }
        if (x0 == x1 && y0 == y1) { break; }
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += step_x; }
        if (twice <= dx) { error += dx; y0 += step_y; }
    }
}

/*
    [修改] 支援任意指定頂點高度的凸多邊形：空心=未觸發，實心=觸發。
    [刪減] 舊版填色只掃描 y=0..12，放大圖形底部會缺少實心區域。
    [新增] 依實際頂點找出包圍框，使用整數叉積判斷內部像素。
    本次六邊形與梯形都是凸多邊形，無需浮點運算與動態記憶體。
*/
/* [新增] 選擇實際 DXF 幾何仍具有的對稱軸；旋轉後不再一律左右鏡射。 */
static const uint8_t IR_MIRROR_X = 0x01U; /* 關於垂直中心線鏡射 */
static const uint8_t IR_MIRROR_Y = 0x02U; /* 關於水平中心線鏡射 */
static void oled_draw_ir_polygon(uint8_t origin_x, uint8_t origin_y, const int8_t (*points)[2],
                                 uint8_t count, bool triggered, uint8_t symmetry) {
    if (points == nullptr || count < 3U) {
        return;
    }

    int min_x = points[0][0];
    int max_x = points[0][0];
    int min_y = points[0][1];
    int max_y = points[0][1];
    for (uint8_t i = 1U; i < count; i++) {
        if (points[i][0] < min_x) { min_x = points[i][0]; }
        if (points[i][0] > max_x) { max_x = points[i][0]; }
        if (points[i][1] < min_y) { min_y = points[i][1]; }
        if (points[i][1] > max_y) { max_y = points[i][1]; }
    }

    if (triggered) {
        for (int y = min_y; y <= max_y; y++) {
            for (int x = min_x; x <= max_x; x++) {
                bool has_positive = false;
                bool has_negative = false;
                for (uint8_t i = 0U; i < count; i++) {
                    uint8_t next = (uint8_t)((i + 1U) % count);
                    int edge_x = (int)points[next][0] - points[i][0];
                    int edge_y = (int)points[next][1] - points[i][1];
                    int point_x = x - points[i][0];
                    int point_y = y - points[i][1];
                    int cross = edge_x * point_y - edge_y * point_x;
                    if (cross > 0) { has_positive = true; }
                    if (cross < 0) { has_negative = true; }
                    if (has_positive && has_negative) { break; }
                }
                if (!has_positive || !has_negative) {
                    int px = (int)origin_x + x;
                    int py = (int)origin_y + y;
                    if (px >= 0 && px < (int)OLED_WIDTH && py >= 0 && py < (int)OLED_HEIGHT) {
                        oled_set_pixel((uint8_t)px, (uint8_t)py, true);
                    }
                }
            }
        }
    }

    /* [保留] 封閉邊框使用整數 Bresenham 畫線。 */
    for (uint8_t i = 0U; i < count; i++) {
        uint8_t next = (uint8_t)((i + 1U) % count);
        oled_draw_segment(origin_x + points[i][0], origin_y + points[i][1],
                          origin_x + points[next][0], origin_y + points[next][1]);
    }

    /*
        [刪減] 移除「兩個圖形一律左右鏡射」；那會使左轉後的 IR1 梯形變形。
        [修改] 左側六邊形兩軸對稱；右側梯形旋轉後只沿水平中心線對稱。
        僅補正 Bresenham 的 1px 量化差異，不改動 DXF 原有斜率與左右輪廓。
    */
    if ((symmetry & IR_MIRROR_X) != 0U) {
        for (int y = min_y; y <= max_y; y++) {
            int py = (int)origin_y + y;
            if (py < 0 || py >= (int)OLED_HEIGHT) { continue; }
            for (int offset = 0; offset <= (max_x - min_x) / 2; offset++) {
                int xl = (int)origin_x + min_x + offset;
                int xr = (int)origin_x + max_x - offset;
                if (xl < 0 || xr >= (int)OLED_WIDTH) { continue; }
                uint16_t li = (uint16_t)(py / 8) * OLED_WIDTH + (uint16_t)xl;
                uint16_t ri = (uint16_t)(py / 8) * OLED_WIDTH + (uint16_t)xr;
                uint8_t mask = (uint8_t)(1U << (py % 8));
                if ((g_oled_buffer[li] & mask) != 0U || (g_oled_buffer[ri] & mask) != 0U) {
                    oled_set_pixel((uint8_t)xl, (uint8_t)py, true);
                    oled_set_pixel((uint8_t)xr, (uint8_t)py, true);
                }
            }
        }
    }
    if ((symmetry & IR_MIRROR_Y) != 0U) {
        for (int x = min_x; x <= max_x; x++) {
            int px = (int)origin_x + x;
            if (px < 0 || px >= (int)OLED_WIDTH) { continue; }
            for (int offset = 0; offset <= (max_y - min_y) / 2; offset++) {
                int yt = (int)origin_y + min_y + offset;
                int yb = (int)origin_y + max_y - offset;
                if (yt < 0 || yb >= (int)OLED_HEIGHT) { continue; }
                uint16_t ti = (uint16_t)(yt / 8) * OLED_WIDTH + (uint16_t)px;
                uint16_t bi = (uint16_t)(yb / 8) * OLED_WIDTH + (uint16_t)px;
                uint8_t tm = (uint8_t)(1U << (yt % 8));
                uint8_t bm = (uint8_t)(1U << (yb % 8));
                if ((g_oled_buffer[ti] & tm) != 0U || (g_oled_buffer[bi] & bm) != 0U) {
                    oled_set_pixel((uint8_t)px, (uint8_t)yt, true);
                    oled_set_pixel((uint8_t)px, (uint8_t)yb, true);
                }
            }
        }
    }
}

/*
    [修正] IR TEST 左右圖形依 IR圖形(1).dxf 的兩組封閉 LWPOLYLINE 重建。
    [修改] 依需求將兩個圖形都向左旋轉 90 度。
    [保留] 各圖形自身對稱性、BAT x.xxxV 右上角、BLE 右下角，不繪製 IR1 / IR2 文字。
*/
static void oled_begin_ir_test_frame(uint8_t battery_percent, uint16_t voltage_mv, bool charging,
                                     bool ir1_triggered, bool ir2_triggered) {
    /*
        [修改] 依 DXF 原始六頂點做「螢幕座標左轉 90 度」轉換：
        new_x = original_top - dxf_y；new_y = original_right - dxf_x。
        統一縮放 3.5 px / DXF 單位，保留左右圖形比例。
        IR2：旋轉後橫向六邊形，左右及上下對稱（16 x 14）。
    */
    static const int8_t hexagon[6][2] = {
        {16, 7}, {12, 14}, {4, 14}, {0, 7}, {4, 0}, {12, 0}
    };
    /*
        [修改] IR1：DXF 右側原始六頂點經相同方向與倍率旋轉。
        輪廓上下對稱，但「左右邊緣長度不同」，不可再強制左右鏡射。
        範圍 x=0..13、y=0..22（13 x 22）。
    */
    static const int8_t trapezoid[6][2] = {
        {0, 4}, {0, 18}, {10, 22}, {13, 20}, {13, 2}, {10, 0}
    };
    /* [新增] IR TEST 僅建立 x.xxV 電壓字串，不帶 BAT 標籤。 */
    char voltage_text[12];
    g_frame_display = {};
    g_frame_battery_percent = battery_percent;
    g_frame_charging = charging;
    g_frame_full = brd_battery_is_full();
    g_frame_ble_connected = brd_bbp_is_connected();
    g_frame_ble_disabled_for_charging = oled_bluetooth_disabled_for_charging(charging);
    g_frame_is_low_battery = false;
    g_frame_is_adc_fault = false;
    g_frame_display_page = BRD_DISPLAY_PAGE_IR_TEST;
    g_frame_ir1_triggered = ir1_triggered;
    g_frame_ir2_triggered = ir2_triggered;
    g_frame_voltage_mv = voltage_mv;
    g_frame_ota = brd_ota_get_display();
    g_warning_frame_drawn = false;

    oled_clear_buffer();
    oled_draw_text(0U, 0U, "IR TEST", 1U);
    /* [修改] 右上角以四捨五入的兩位小數顯示電池電壓，取代先前空白區。 */
    oled_format_ir_voltage(voltage_mv, voltage_text, sizeof(voltage_text));
    oled_draw_right_aligned_text((uint8_t)(OLED_WIDTH - 1U), 0U, voltage_text, 1U);

    /*
        [修改] IR TEST 圖示水平間距由 18 px 縮小為 10 px。
        [修改] IR2 右移 4 px：中心 (50,19)，x=42..58、y=12..26。
        [修改] IR1 左移 4 px：中心 (75.5,19)，x=69..82、y=8..30。
        [保留] DXF 頂點、尺寸、旋轉方向、各自鏡射規則與其他畫面內容。
        [刪減] 舊位置 IR2 x=38、IR1 x=73。
        兩外框間 x=59..68，共 10 px 空白。
    */
    oled_draw_ir_polygon(42U, 12U, hexagon, 6U, ir2_triggered,
                         IR_MIRROR_X | IR_MIRROR_Y);
    oled_draw_ir_polygon(69U, 8U, trapezoid, 6U, ir1_triggered, IR_MIRROR_Y);

    /* [保留] IR TEST 頁面沒有電池圖示；BLE 狀態仍在右下角。 */
    if (g_frame_ble_disabled_for_charging) {
        oled_draw_bluetooth_disabled_text();
    } else if (g_frame_ble_connected) {
        oled_draw_bluetooth_icon();
    }

    g_page = 0U;
    g_column = 0U;
    g_page_address_pending = true;
    g_frame_active = true;
    g_last_frame_start_ms = millis();
}

static void oled_begin_frame(const brd_display_t &display, uint8_t battery_percent, bool charging) {
    char value_line[16];
    /* [V1.4 OLED 刪除] 主頁不再建立 battery_text：僅保留電池圖形作為 SOC 指示。 */

    g_frame_display = display;
    /* [新增] 普通量測頁包括 OTA，標記 MAIN 避免切換比較誤判。 */
    g_frame_display_page = BRD_DISPLAY_PAGE_MAIN;
    g_frame_battery_percent = battery_percent;
    g_frame_voltage_mv = brd_battery_get_voltage_mv();
    g_frame_charging = charging;
    g_frame_full = brd_battery_is_full();
    g_frame_ble_connected = brd_bbp_is_connected();
    g_frame_ble_disabled_for_charging = oled_bluetooth_disabled_for_charging(charging);
    g_frame_is_low_battery = false;
    g_frame_is_adc_fault = false;
    g_warning_frame_drawn = false;
    oled_clear_buffer();
    /* [V1.4 OLED UI 修改] LOAD 模式只顯示 WAIT LOAD / LOADED。
       [V1.4 OLED UI 刪除] 主頁的 LOADED READY 與 HOLD 字串顯示；
       [保留] MAX 的自鎖時間與數值顯示，由 brd_measurement 原機制控制。
       [保留] AUTO READY、IR ERR/LOSS 與 OTA 專用狀態。 */
    g_frame_ota = brd_ota_get_display();
    const char *status_line = g_frame_ota.active ?
        (g_frame_ota.verifying ? "VERIFY" : (g_frame_ota.phase == brd_ota::COMPLETE ? "OTA DONE" : "OTA UPDATE")) :
        (display.input_fault ? "IR ERR" : (display.sample_loss ? "IR LOSS" :
        (brd_settings_get().mode == BRD_MEASUREMENT_MODE_AUTO ? "AUTO READY" :
         (display.loaded ? "LOADED" : "WAIT LOAD"))));
    oled_draw_text(0U, 0U, status_line, 1U);
    if (g_frame_ota.active) {
        /* [V1.2 刪減] 原本 "0~100 PCT" 字串與左對齊的第二行數值顯示。 */
        /* [V1.2 新增] OTA 專用版面：左側扇形圓盤，百分比置於 OLED 水平中央。 */
        snprintf(value_line, sizeof(value_line), "%u%%", static_cast<unsigned>(g_frame_ota.percent));
        oled_draw_ota_sector(13U, 23U, 8U, g_frame_ota.percent);
        oled_draw_centered_text(16U, value_line, 2U);
    } else {
        /* [V1.4 OLED 刪減] 移除 SP 後新增的額外空白，回復 "SP 15800"；
           [保留] MAX 顯示 "MAX 18800"、x0 / y16 / 2 倍字體及轉速值。 */
        snprintf(value_line, sizeof(value_line), display.show_max ? "MAX %u" : "SP %u",
                 (unsigned int)display.value);
        oled_draw_text(0U, 16U, value_line, 2U);
    }
    /* [修改] 充電停用分兩行顯示 BLE / OFF 小字；一般連線沿用原生藍牙符號。
       [刪減] 原本只判斷已連線，充電停用後右下角完全空白的顯示方式。 */
    if (g_frame_ble_disabled_for_charging) {
        oled_draw_bluetooth_disabled_text();
    } else if (g_frame_ble_connected) {
        oled_draw_bluetooth_icon();
    }
    /* [V1.4 OLED 修改] 右上角僅顯示「閃電 -> 電池」，不顯示任何 SOC 數字或 %。
       [保留] battery_percent 僅供電池圖案填充；電池演算法、低電保護、BLE 電量封包皆不修改。 */
    const uint8_t visible_percent = battery_percent > 100U ? 100U : battery_percent;

    /* [V1.4 OLED 修改] 所有狀態均以實際 SOC 繪製靜態電池，不使用動畫相位。
       [保留] 充電中且尚未 FULL 時顯示左側閃電；確認 FULL 後閃電熄滅。 */
    oled_draw_battery_icon(OLED_MAIN_BATTERY_X, 0U, visible_percent);
    if (charging && !g_frame_full) {
        oled_draw_charging_icon(OLED_MAIN_LIGHTNING_X, 1U);
    }

    g_page = 0U;
    g_column = 0U;
    g_page_address_pending = true;
    g_frame_active = true;
    g_last_frame_start_ms = millis();
}

static bool oled_send_frame_step(void) {
    if (g_page_address_pending) {
        const uint8_t commands[] = {0x00U, (uint8_t)(0xB0U + g_page), 0x00U, 0x10U};
        if (!oled_transmit(commands, sizeof(commands))) {
            return false;
        }
        g_page_address_pending = false;
        return true;
    }

    uint8_t packet[OLED_I2C_DATA_CHUNK_SIZE + 1U];
    size_t length = OLED_WIDTH - g_column;
    if (length > OLED_I2C_DATA_CHUNK_SIZE) {
        length = OLED_I2C_DATA_CHUNK_SIZE;
    }
    packet[0] = 0x40U;
    memcpy(&packet[1], &g_oled_buffer[(uint16_t)g_page * OLED_WIDTH + g_column], length);
    if (!oled_transmit(packet, length + 1U)) {
        return false;
    }

    g_column += (uint16_t)length;
    if (g_column >= OLED_WIDTH) {
        g_column = 0U;
        g_page++;
        g_page_address_pending = true;
    }
    if (g_page >= OLED_HEIGHT / 8U) {
        g_frame_active = false;
    }
    return true;
}

void brd_oled_begin(void) {
    g_last_retry_ms = millis();
    g_oled_available = oled_initialize();
    if (!g_oled_available) {
        oled_mark_failed();
        return;
    }

    if (brd_battery_is_low_locked()) {
        /* [新增] 上電即低電時不播放開機畫面，直接顯示沒電圖示。 */
        oled_begin_low_battery_frame();
    } else if (brd_battery_is_adc_fault()) {
        /* [V0.12 新增] ADC 初始化失敗時略過開機等待，立即顯示 ADC ERR。 */
        oled_begin_adc_fault_frame();
    } else {
        /* [保留] 開機時中斷尚未啟動，可以完整送出版本畫面。 */
        oled_clear_buffer();
        /* [修改] 頂部大字專案名稱；下方小字韌體版本。 */
        /* [刪減] 頂部小字 BRD 與下方混合名稱/版本的大字。 */
        oled_draw_centered_text(1U, PROJECT_BOOT_NAME, 2U);
        oled_draw_centered_text(23U, PROJECT_BOOT_VERSION, 1U);
        g_page = 0U;
        g_column = 0U;
        g_page_address_pending = true;
        g_frame_active = true;
        g_frame_is_low_battery = false;
        g_frame_is_adc_fault = false;
    }

    while (g_frame_active) {
        if (!oled_send_frame_step()) {
            oled_mark_failed();
            return;
        }
    }
    if (g_frame_is_low_battery || g_frame_is_adc_fault) {
        g_warning_frame_drawn = true;
        g_last_warning_frame_ms = millis();
        return;
    }
    delay(OLED_BOOT_VERSION_DISPLAY_MS);
    g_last_frame_start_ms = millis() - OLED_UPDATE_INTERVAL_MS;
}

void brd_oled_update(void) {
    brd_ota_display_t ota = brd_ota_get_display();
    /* [V1.2 新增] OTA 進出與 VERIFY/DONE 切換即放棄上一幅未完成畫面。 */
    if (ota.active != g_frame_ota.active || ota.verifying != g_frame_ota.verifying || ota.phase != g_frame_ota.phase) {
        g_frame_active = false;
    }
    bool low_locked = brd_battery_is_low_locked();
    bool adc_fault = !low_locked && brd_battery_is_adc_fault();
    /* [新增] 頁面切換不需等前一幅 frame 完成，OTA / 電池警告仍最高優先。 */
    if (!ota.active && !low_locked && !adc_fault &&
        brd_settings_ble_display_page() != g_frame_display_page) {
        g_frame_active = false;
    }
    /* [V0.12 修改] 切換警示模式或從 ADC 故障恢復時，放棄舊畫面。 */
    if (low_locked != g_frame_is_low_battery || adc_fault != g_frame_is_adc_fault) {
        /* [新增] 放棄舊模式的未完成畫面，下一幅依低電、ADC 故障或正常模式重畫。 */
        g_frame_active = false;
        g_warning_frame_drawn = false;
    }

    if (!g_oled_available) {
        if ((uint32_t)(millis() - g_last_retry_ms) < OLED_RETRY_INTERVAL_MS) {
            return;
        }
        g_last_retry_ms = millis();
        g_oled_available = oled_initialize();
        if (!g_oled_available) {
            oled_mark_failed();
            return;
        }
        g_last_frame_start_ms = millis() - OLED_UPDATE_INTERVAL_MS;
        /* [V0.10 修改] 復原後依低電鎖定狀態畫警示或量測資料，不重播開機畫面。 */
        return;
    }

    if (low_locked || adc_fault) {
        if (g_warning_frame_drawn &&
            (uint32_t)(millis() - g_last_warning_frame_ms) < BATTERY_LOW_OLED_REFRESH_MS) {
            return;
        }

        /*
            [新增] 量測中斷已停用，直接送完警示，避免一般畫面分段等待延後顯示。
            每個 I2C 封包仍有 timeout；任一包失敗即退出，沿用 OLED 重試機制。
        */
        if (low_locked) {
            oled_begin_low_battery_frame();
        } else {
            oled_begin_adc_fault_frame();
        }
        while (g_frame_active) {
            if (!oled_send_frame_step()) {
                oled_mark_failed();
                return;
            }
        }
        g_warning_frame_drawn = true;
        g_last_warning_frame_ms = millis();
        return;
    }

    if (!g_frame_active) {
        brd_display_t display = brd_measurement_get_display();
        uint8_t battery_percent = brd_battery_get_percent();
        uint16_t voltage_mv = brd_battery_get_voltage_mv();
        bool charging = brd_io_is_charging();
        uint8_t requested_page = brd_settings_ble_display_page();
        bool ir1_triggered = brd_io_read_ir1();
        bool ir2_triggered = brd_io_read_ir2();
        bool changed_state = (requested_page != g_frame_display_page) ||
            ota.active != g_frame_ota.active || ota.verifying != g_frame_ota.verifying ||
            ota.phase != g_frame_ota.phase || ota.percent != g_frame_ota.percent ||
            battery_percent != g_frame_battery_percent || charging != g_frame_charging ||
            (requested_page == BRD_DISPLAY_PAGE_MAIN &&
             brd_battery_is_full() != g_frame_full) ||
            brd_bbp_is_connected() != g_frame_ble_connected ||
            oled_bluetooth_disabled_for_charging(charging) != g_frame_ble_disabled_for_charging;
        if (requested_page == BRD_DISPLAY_PAGE_IR_TEST && !ota.active) {
            /* [新增] IR TEST 不追蹤量測 generation，避免高速 RPM 造成不必要重畫。 */
            changed_state = changed_state || voltage_mv != g_frame_voltage_mv ||
                ir1_triggered != g_frame_ir1_triggered || ir2_triggered != g_frame_ir2_triggered;
        } else {
            /* [保留] 主頁及 OTA 繼續依既有量測結果刷新。 */
            changed_state = changed_state || display.generation != g_frame_display.generation ||
                display.loaded != g_frame_display.loaded || display.show_max != g_frame_display.show_max ||
                display.hold_active != g_frame_display.hold_active;
        }
        if (!changed_state &&
            (uint32_t)(millis() - g_last_frame_start_ms) < OLED_UPDATE_INTERVAL_MS) {
            return;
        }
        if (!ota.active && requested_page == BRD_DISPLAY_PAGE_IR_TEST) {
            oled_begin_ir_test_frame(battery_percent, voltage_mv, charging,
                                     ir1_triggered, ir2_triggered);
        } else {
            oled_begin_frame(display, battery_percent, charging);
        }
    }

    /* [V0.10 新增] 一般畫面每次只送一包，下一次 loop 先處理量測再送下一包。 */
    if (!oled_send_frame_step()) {
        oled_mark_failed();
        return;
    }

    if (!g_frame_active && g_frame_display.show_max && !g_frame_ota.active) {
        brd_measurement_max_frame_presented(g_frame_display.generation);
    }
}
