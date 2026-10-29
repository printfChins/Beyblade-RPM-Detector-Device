/*
 * File: BRD_BPX_PRO/brd_ui_legible_text.cpp
 * [新增] 使用現成 olikraus/U8g2_for_Adafruit_GFX 字型轉接器。
 * [刪減] 無手工 glyph/bitmaps、無自行縮放；全部交由字型庫負責。
 * [設計] 只快取非擁有的 display 指標；支援 TFT 與 GFXcanvas16。
 * [相依] Arduino Library Manager: U8g2_for_Adafruit_GFX。
 */
#include "brd_ui_legible_text.h"
#include <U8g2_for_Adafruit_GFX.h>

namespace brd_ui { namespace legible_text {
namespace {

// [新增] 直接引用 U8g2 發行版預定義字型，不複製任何字形資料。
const uint8_t *select_font(Density density) {
    return density == Density::Compact
        ? u8g2_font_6x10_tr
        : u8g2_font_7x13_tr;
}

// [新增] UI 單執行緒共用一個 U8g2 轉接器，避免每筆文字重複建構。
// 畫面切換時若轉向其他 Adafruit_GFX／GFXcanvas16 物件才重新綁定。
U8G2_FOR_ADAFRUIT_GFX &adapter(Adafruit_GFX &gfx) {
    static U8G2_FOR_ADAFRUIT_GFX font;
    static Adafruit_GFX *bound_gfx = nullptr;
    if (bound_gfx != &gfx) {
        font.begin(gfx);
        bound_gfx = &gfx;
    }
    return font;
}

} // namespace

int width(Adafruit_GFX &gfx, const char *text, Density density) {
    if (text == nullptr || *text == '\0') return 0;

    U8G2_FOR_ADAFRUIT_GFX &font = adapter(gfx);
    font.setFont(select_font(density));
    return static_cast<int>(font.getUTF8Width(text));
}

void draw(Adafruit_GFX &gfx, int x, int y, const char *text,
          uint16_t color, Density density) {
    if (text == nullptr || *text == '\0') return;

    U8G2_FOR_ADAFRUIT_GFX &font = adapter(gfx);
    font.setFontMode(1);  // [保留] 透明文字，畫面背景由場景自行繪製。
    font.setFontDirection(0);
    font.setFont(select_font(density));
    font.setForegroundColor(color);
    // [修改] 原場景使用上緣 Y；U8g2 的 Y 則是字體 baseline。
    font.setCursor(static_cast<int16_t>(x),
                   static_cast<int16_t>(y + font.getFontAscent()));
    font.print(text);
}

} } // namespace brd_ui::legible_text
