/* 檔案: BRD_BPX_PRO/brd_ui_screen.cpp
 * [移動] ST7735 初始化、背光 GPIO 由舊 brd_tft.cpp 移至此處。
 * [保留] 官方函式庫、CS=GND(-1)、外部 RESET RC(-1)、SPI 8 MHz。
 */
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <driver/gpio.h>
#include <cstring>
#include "brd_config.h"
#include "brd_ui_screen.h"
namespace brd_ui {
namespace {
Adafruit_ST7735 display(&SPI, -1, TFT_DC_GPIO, -1);
}
Adafruit_ST7735 &lcd() { return display; }
void panel_backlight(bool on) {
    const uint8_t on_level = TFT_BACKLIGHT_ON_LEVEL;
    const uint8_t level = on ? on_level : (on_level == HIGH ? LOW : HIGH);
    (void)gpio_set_level((gpio_num_t)TFT_BACKLIGHT_GPIO, level);
}
bool panel_begin() {
    /* [注意] RC RESET 與 CS 常低可能影響冷開機；不宣稱能靠初始化偵測實體 LCD。 */
    pinMode(TFT_BACKLIGHT_GPIO, OUTPUT);
    (void)gpio_set_pull_mode((gpio_num_t)TFT_BACKLIGHT_GPIO, GPIO_FLOATING);
    panel_backlight(false);
    delay(TFT_POWER_STABLE_WAIT_MS);
    SPI.begin(TFT_SPI_SCK_GPIO, -1, TFT_SPI_MOSI_GPIO, -1);
    (void)gpio_set_pull_mode((gpio_num_t)TFT_SPI_SCK_GPIO, GPIO_FLOATING);
    (void)gpio_set_pull_mode((gpio_num_t)TFT_SPI_MOSI_GPIO, GPIO_FLOATING);
    display.initR(TFT_ADAFRUIT_INIT_PROFILE);
    delay(TFT_REINIT_GAP_MS);
    display.initR(TFT_ADAFRUIT_INIT_PROFILE);
    display.setSPISpeed(TFT_SPI_FREQUENCY_HZ);
    display.setRotation(TFT_ROTATION);
    // [新增] 只修正 ST7735 面板的 RGB/BGR 色彩順序。
    // Arduino Adafruit MINI profile 預設 RGB；部分 160x80 IPS 面板需要 BGR。
    // 旋轉座標維持由官方 setRotation() 管理，這裡沿用相同方向位元，
    // 僅增加 MADCTL_BGR(0x08)，避免更換 INITR profile 造成畫面偏移。
#if TFT_PANEL_BGR_ORDER
    uint8_t madctl = ST7735_MADCTL_BGR;
    switch (TFT_ROTATION & 3U) {
    case 0U:
        madctl |= ST77XX_MADCTL_MX | ST77XX_MADCTL_MY;
        break;
    case 1U:
        madctl |= ST77XX_MADCTL_MY | ST77XX_MADCTL_MV;
        break;
    case 2U:
        break;
    case 3U:
        madctl |= ST77XX_MADCTL_MX | ST77XX_MADCTL_MV;
        break;
    }
    display.sendCommand(ST77XX_MADCTL, &madctl, 1U);
#endif
    display.invertDisplay(TFT_INVERSION_ON != 0U);
    display.setTextWrap(false);
    return display.width() == (int16_t)TFT_WIDTH &&
        display.height() == (int16_t)TFT_HEIGHT;
}
void clear_region(const Rect &rect, uint16_t color) {
    if (rect.w <= 0 || rect.h <= 0) { return; }
    /* [新增] 限制元件清除區域不超出 160x80；不清除其他元件。 */
    const int32_t x0 = rect.x > 0 ? rect.x : 0;
    const int32_t y0 = rect.y > 0 ? rect.y : 0;
    const int32_t x1 = (int32_t)rect.x + rect.w < (int32_t)TFT_WIDTH ?
        (int32_t)rect.x + rect.w : TFT_WIDTH;
    const int32_t y1 = (int32_t)rect.y + rect.h < (int32_t)TFT_HEIGHT ?
        (int32_t)rect.y + rect.h : TFT_HEIGHT;
    if (x1 > x0 && y1 > y0) {
        display.fillRect((int16_t)x0, (int16_t)y0,
                         (int16_t)(x1 - x0), (int16_t)(y1 - y0), color);
    }
}
void label(int16_t x, int16_t y, const char *text, uint8_t scale, uint16_t color) {
    if (text == nullptr || scale == 0U) { return; }
    display.setTextSize(scale);
    display.setTextColor(color);
    display.setCursor(x, y);
    display.print(text);
}
void label_opaque(int16_t x, int16_t y, char c, uint8_t scale,
                  uint16_t color, uint16_t background) {
    char s[2] = {c, '\0'};
    display.setTextSize(scale);
    display.setTextColor(color, background);
    display.setCursor(x, y);
    display.print(s);
}
int16_t text_width(const char *text, uint8_t scale) {
    return text == nullptr ? 0 : (int16_t)(strlen(text) * 6U * scale);
}
void label_right(int16_t y, const char *text, uint8_t scale, uint16_t color) {
    const int16_t x = (int16_t)TFT_WIDTH - text_width(text, scale) - 2;
    label(x > 0 ? x : 0, y, text, scale, color);
}
void label_center(int16_t y, const char *text, uint8_t scale, uint16_t color) {
    const int16_t x = ((int16_t)TFT_WIDTH - text_width(text, scale)) / 2;
    label(x > 0 ? x : 0, y, text, scale, color);
}
} // namespace brd_ui
