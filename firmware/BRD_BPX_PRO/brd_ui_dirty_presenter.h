/*
 * File: BRD_BPX_PRO/brd_ui_dirty_presenter.h
 * [新增] 8x16 Tile Dirty Diff + GFXcanvas16, 不直接操作 ST7735 寄存器。
 * [設計] 25,600 bytes 畫布 + 5,120 bytes 傳輸暫存 + 400 bytes Hash。
 */
#ifndef BRD_UI_DIRTY_PRESENTER_H
#define BRD_UI_DIRTY_PRESENTER_H
#include <stdint.h>
#include <Adafruit_GFX.h>
class Adafruit_ST7735;
namespace brd_ui { namespace dirty_presenter {
constexpr uint8_t TILE_W=8U, TILE_H=16U;
constexpr uint8_t TILE_COLS=20U, TILE_ROWS=5U;
bool begin();
void invalidate();
Adafruit_GFX* canvas();
// [ADD] RGB565 framebuffer access only for in-place transition compositor.
uint16_t* pixels();
uint32_t flush(Adafruit_ST7735 &lcd);
uint32_t last_bytes();
uint32_t total_bytes();
uint16_t last_regions();
} }
#endif
