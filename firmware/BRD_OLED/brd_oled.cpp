/*
    檔案位置: BRD_OLED/brd_oled.cpp
    [V0.10 修改] 顯示 LOAD 狀態、RPM / MAX 與開機版本。
    [V0.10 恢復] 右上角電池圖示與左側充電閃電圖示，沿用 V1.9 尺寸與位置。
    [V0.10 刪減] OLED 休眠與獨立 FreeRTOS Task。
    [V0.10 修改] 不使用 Wire.begin，避免框架初始化自動開啟內部上拉。
    [V0.10 新增] I2C 明確不開內部上拉；初始化或傳送失敗每隔一秒重試。
    [V0.10 新增] 一般畫面分段寫入，每次 loop 最多一個 I2C 傳送。
    [V0.10 修改] 低電優先顯示圓角電池、左側短條與中央閃電圖示。
    [V0.10 新增] MAX 自鎖期間第一行顯示 HOLD，自鎖結束恢復裝載狀態。
    狀態與畫面皆在主 loop 存取，不再有跨 Task 同時讀寫量測資料的問題。
*/
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <cstring>
#include <cstdio>

#include "brd_battery.h"
#include "brd_config.h"
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
static bool g_frame_is_low_battery = false;
static bool g_low_battery_frame_drawn = false;
static uint32_t g_last_low_battery_frame_ms = 0UL;

static void oled_mark_failed(void) {
    g_oled_available = false;
    g_frame_active = false;
    g_low_battery_frame_drawn = false;
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
    static const uint8_t glyph_D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t glyph_E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
    /* [V0.10 新增] HOLD 所需 H 字元，其餘 O、L、D 沿用既有字庫。 */
    static const uint8_t glyph_H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
    static const uint8_t glyph_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
    static const uint8_t glyph_L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t glyph_M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    static const uint8_t glyph_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
    static const uint8_t glyph_P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t glyph_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const uint8_t glyph_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    /* [保留] PROJECT_VERSION 顯示所需 V 字元。 */
    static const uint8_t glyph_V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
    static const uint8_t glyph_W[5] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
    /* [保留] MAX 標籤所需 X 字元。 */
    static const uint8_t glyph_X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t glyph_Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
    /* [保留] PROJECT_VERSION 小數點。 */
    static const uint8_t glyph_dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};

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
        case 'D': return glyph_D;
        case 'E': return glyph_E;
        case 'H': return glyph_H;
        case 'I': return glyph_I;
        case 'L': return glyph_L;
        case 'M': return glyph_M;
        case 'O': return glyph_O;
        case 'P': return glyph_P;
        case 'R': return glyph_R;
        case 'T': return glyph_T;
        case 'V': return glyph_V;
        case 'W': return glyph_W;
        case 'X': return glyph_X;
        case 'Y': return glyph_Y;
        case '.': return glyph_dot;
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
    g_page = 0U;
    g_column = 0U;
    g_page_address_pending = true;
    g_frame_active = true;
}

static void oled_begin_frame(const brd_display_t &display, uint8_t battery_percent, bool charging) {
    char value_line[16];

    g_frame_display = display;
    g_frame_battery_percent = battery_percent;
    g_frame_charging = charging;
    g_frame_is_low_battery = false;
    g_low_battery_frame_drawn = false;
    oled_clear_buffer();
    /* [V0.10 刪減] 自鎖期間仍只顯示 WAIT LOAD 的判斷方式。
       [V0.10 新增] HOLD 優先顯示；第二行仍保留本次 MAX 轉速。 */
    const char *status_line = display.hold_active ? "HOLD" :
        (display.loaded ? "LOADED READY" : "WAIT LOAD");
    oled_draw_text(0U, 0U, status_line, 1U);
    snprintf(value_line, sizeof(value_line), "%s %u",
             display.show_max ? "MAX" : "RPM", (unsigned int)display.value);
    oled_draw_text(0U, 16U, value_line, 2U);
    /* [V0.10 恢復] 電量與充電圖示皆位於第一行右側，不占用 RPM / MAX 數值區。 */
    oled_draw_battery_icon(108U, 0U, battery_percent);
    if (charging) {
        oled_draw_charging_icon(100U, 1U);
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
    } else {
        /* [保留] 開機時中斷尚未啟動，可以完整送出版本畫面。 */
        oled_clear_buffer();
        oled_draw_centered_text(1U, PROJECT_SHORT_NAME, 1U);
        oled_draw_centered_text(16U, PROJECT_VERSION, 2U);
        g_page = 0U;
        g_column = 0U;
        g_page_address_pending = true;
        g_frame_active = true;
        g_frame_is_low_battery = false;
    }

    while (g_frame_active) {
        if (!oled_send_frame_step()) {
            oled_mark_failed();
            return;
        }
    }
    if (g_frame_is_low_battery) {
        g_low_battery_frame_drawn = true;
        g_last_low_battery_frame_ms = millis();
        return;
    }
    delay(OLED_BOOT_VERSION_DISPLAY_MS);
    g_last_frame_start_ms = millis() - OLED_UPDATE_INTERVAL_MS;
}

void brd_oled_update(void) {
    bool low_locked = brd_battery_is_low_locked();
    if (low_locked && !g_frame_is_low_battery) {
        /* [新增] 立即放棄尚未送完的一般畫面，下一幅只顯示沒電警示。 */
        g_frame_active = false;
        g_low_battery_frame_drawn = false;
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

    if (low_locked) {
        if (g_low_battery_frame_drawn &&
            (uint32_t)(millis() - g_last_low_battery_frame_ms) < BATTERY_LOW_OLED_REFRESH_MS) {
            return;
        }

        /*
            [新增] 量測中斷已停用，直接送完警示，避免一般畫面分段等待延後顯示。
            每個 I2C 封包仍有 timeout；任一包失敗即退出，沿用 OLED 重試機制。
        */
        oled_begin_low_battery_frame();
        while (g_frame_active) {
            if (!oled_send_frame_step()) {
                oled_mark_failed();
                return;
            }
        }
        g_low_battery_frame_drawn = true;
        g_last_low_battery_frame_ms = millis();
        return;
    }

    if (!g_frame_active) {
        brd_display_t display = brd_measurement_get_display();
        uint8_t battery_percent = brd_battery_get_percent();
        bool charging = brd_io_is_charging();
        bool changed_state = display.generation != g_frame_display.generation ||
            display.loaded != g_frame_display.loaded || display.show_max != g_frame_display.show_max ||
            /* [V0.10 新增] 自鎖結束即要求重畫，即使 MAX 數值及電量都未改變。 */
            display.hold_active != g_frame_display.hold_active ||
            battery_percent != g_frame_battery_percent || charging != g_frame_charging;
        if (!changed_state &&
            (uint32_t)(millis() - g_last_frame_start_ms) < OLED_UPDATE_INTERVAL_MS) {
            return;
        }
        oled_begin_frame(display, battery_percent, charging);
    }

    /* [V0.10 新增] 一般畫面每次只送一包，下一次 loop 先處理量測再送下一包。 */
    if (!oled_send_frame_step()) {
        oled_mark_failed();
        return;
    }

    if (!g_frame_active && g_frame_display.show_max) {
        brd_measurement_max_frame_presented(g_frame_display.generation);
    }
}
