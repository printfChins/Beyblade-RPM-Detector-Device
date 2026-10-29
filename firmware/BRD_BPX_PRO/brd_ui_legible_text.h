/*
 * File: BRD_BPX_PRO/brd_ui_legible_text.h
 * [修改] 使用現成 U8g2_for_Adafruit_GFX 字型庫，不再維護自製字形表。
 * [新增] Regular: u8g2_font_7x13_tr，Compact: u8g2_font_6x10_tr。
 * [保留] Adafruit_GFX / GFXcanvas16 / RGB565 繪製介面。
 */
#ifndef BRD_UI_LEGIBLE_TEXT_H
#define BRD_UI_LEGIBLE_TEXT_H

#include <Adafruit_GFX.h>
#include <cstdint>

namespace brd_ui { namespace legible_text {

enum class Density : uint8_t { Regular, Compact };

// [修改] 由 U8g2 字型庫取得實際 UTF-8 字串寬度；非固定字距推算。
int width(Adafruit_GFX &gfx, const char *text,
          Density density = Density::Regular);

// [修改] x/y 仍為左上角座標；內部轉成 U8g2 的 baseline。
void draw(Adafruit_GFX &gfx, int x, int y, const char *text,
          uint16_t color, Density density = Density::Regular);

} } // namespace brd_ui::legible_text
#endif // BRD_UI_LEGIBLE_TEXT_H
