/* 檔案: BRD_BPX_PRO/brd_ui_screen.h
 * [新增] ST7735 硬體唯一持有者；不自行實作 LCD 指令或 SPI 畫面傳輸。
 * 畫面元件只呼叫 Adafruit GFX 公開 API。
 */
#ifndef BRD_UI_SCREEN_H
#define BRD_UI_SCREEN_H
#include <stdint.h>
#include <Adafruit_ST7735.h>
namespace brd_ui {
namespace palette {
constexpr uint16_t bg = 0x0841U;
constexpr uint16_t white = ST77XX_WHITE;
constexpr uint16_t cyan = ST77XX_CYAN;
constexpr uint16_t blue = 0x0412U;
constexpr uint16_t panel = 0x1082U;
constexpr uint16_t gray = 0x8410U;
constexpr uint16_t green = ST77XX_GREEN;
constexpr uint16_t yellow = ST77XX_YELLOW;
constexpr uint16_t red = ST77XX_RED;
}
struct Rect { int16_t x, y, w, h; };
bool panel_begin();
Adafruit_ST7735 &lcd();
void panel_backlight(bool on);
void clear_region(const Rect &rect, uint16_t color = palette::bg);
void label(int16_t x, int16_t y, const char *text, uint8_t scale, uint16_t color);
void label_opaque(int16_t x, int16_t y, char character, uint8_t scale,
                  uint16_t color, uint16_t background);
int16_t text_width(const char *text, uint8_t scale);
void label_right(int16_t y, const char *text, uint8_t scale, uint16_t color);
void label_center(int16_t y, const char *text, uint8_t scale, uint16_t color);
}
#endif
