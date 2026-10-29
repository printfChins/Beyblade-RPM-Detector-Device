/*
 * [測試替身] U8g2 介面簽章，不包含任何正式字型資料。
 * 正式 Arduino 韌體需安裝 olikraus/U8g2_for_Adafruit_GFX。
 * 注意：主機端預覽字形由 Adafruit GFX stub 繪製，不是 U8g2 真實字形。
 */
#ifndef BRD_TEST_U8G2_FOR_ADAFRUIT_GFX_H
#define BRD_TEST_U8G2_FOR_ADAFRUIT_GFX_H
#include <Adafruit_GFX.h>
#include <cstdint>
#include <cstring>

static const uint8_t u8g2_font_7x13_tr[] = {0x07U};
static const uint8_t u8g2_font_6x10_tr[] = {0x06U};

class U8G2_FOR_ADAFRUIT_GFX {
public:
    void begin(Adafruit_GFX &gfx) {gfx_ = &gfx;}
    void setFontMode(uint8_t) {}
    void setFontDirection(uint8_t) {}
    void setFont(const uint8_t *font) {font_ = font;}
    void setForegroundColor(uint16_t value) {color_ = value;}
    int8_t getFontAscent() const {return font_ == u8g2_font_6x10_tr ? 8 : 11;}
    int8_t getFontDescent() const {return font_ == u8g2_font_6x10_tr ? -2 : -2;}
    int16_t getUTF8Width(const char *text) const {
        return static_cast<int16_t>(std::strlen(text) * (font_ == u8g2_font_6x10_tr ? 6U : 7U));
    }
    void setCursor(int16_t x,int16_t baseline) {x_ = x; baseline_ = baseline;}
    void print(const char *text) {
        if(!gfx_) return;
        gfx_->setTextSize(1);
        gfx_->setTextColor(color_);
        gfx_->setCursor(x_, static_cast<int16_t>(baseline_-getFontAscent()));
        gfx_->print(text);
    }
private:
    Adafruit_GFX *gfx_ = nullptr;
    const uint8_t *font_ = u8g2_font_7x13_tr;
    uint16_t color_ = 0xffffU;
    int16_t x_ = 0;
    int16_t baseline_ = 0;
};
#endif
