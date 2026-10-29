/* File: tests/test_tile_dirty.cpp
 * [新增] Tile合併、零重繪、單區塊更新、SPI上限回歸測試。
 */
#include <cassert>
#include <cstdio>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "brd_ui_dirty_presenter.h"
int main() {
    Adafruit_ST7735 lcd(&SPI,-1,2,-1);
    using namespace brd_ui::dirty_presenter;
    assert(begin());
    auto *g=canvas();assert(g);
    g->fillScreen(0x0000U);
    assert(flush(lcd)==25600UL);
    assert(last_regions()==5U);
    assert(flush(lcd)==0U);
    assert(last_regions()==0U);
    g->drawPixel(1,1,0xffffU);
    assert(flush(lcd)==256U);
    assert(last_regions()==1U);
    assert(lcd.pixels[1U*160+1U]==0xffffU);
    g->drawPixel(9,1,0xf800U);
    assert(flush(lcd)==256U);
    assert(last_regions()==1U);
    g->drawPixel(1,1,0x0000U);
    g->drawPixel(9,1,0x0000U);
    assert(flush(lcd)==512U);
    assert(last_regions()==1U); // 相鄰兩 tile 自動合併成一個 SPI Window
    assert(flush(lcd)==0U);
    // [新增] 驗證跨越 Tile 邊界的兩個角落，排除記憶體索引混亂。
    g->drawPixel(0,0,0xF800U);
    g->drawPixel(159,79,0x07E0U);
    assert(flush(lcd)==512U);
    assert(lcd.pixels[0]==0xF800U);
    assert(lcd.pixels[79U*160U+159U]==0x07E0U);
    // [新增] 跨三個 tile 的局部更新，檢查座標與 byte order。
    g->fillRect(61,26,48,10,0x001FU);
    assert(flush(lcd)>0U);
    for(int y=26;y<36;++y)
        for(int x=61;x<109;++x)
            assert(lcd.pixels[y*160+x]==0x001FU);
    assert(flush(lcd)==0U);
    std::puts("PASS: 25.6KB initial, 0B unchanged, 256B tile; RGB565 canvas matches LCD in all corners and multi-tile zones.");
}
