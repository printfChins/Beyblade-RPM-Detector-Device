/*
 * File: BRD_BPX_PRO/brd_ui_dirty_presenter.cpp
 * [新增] 單畫布 Tile Hash Dirty Region，避免每幀全畫面 SPI 重送。
 * [修改] 改由官方 drawRGBBitmap() 傳送變動矩形，讓函式庫處理像素順序與逐列傳輸。
 * [保留] 直接 SPI 阻塞傳輸只在畫面更新時進行；RPM ISR 不觸發繪圖。
 */
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <new>
#include <cstring>
#include "brd_ui_dirty_presenter.h"
namespace brd_ui { namespace dirty_presenter {
namespace {
constexpr uint16_t WIDTH=160U, HEIGHT=80U;
constexpr uint16_t TILE_PIXELS=TILE_W*TILE_H;
GFXcanvas16 *framebuffer=nullptr;
uint16_t *tx_buffer=nullptr;
uint32_t previous_hash[TILE_COLS*TILE_ROWS]={};
bool valid[TILE_COLS*TILE_ROWS]={};
uint32_t last_transfer=0U;
uint32_t total_transfer=0U;
uint16_t region_count=0U;
uint32_t hash_tile(const uint16_t *frame,uint8_t tx,uint8_t ty) {
    uint32_t h=2166136261UL;
    const uint16_t px=(uint16_t)tx*TILE_W;
    const uint16_t py=(uint16_t)ty*TILE_H;
    for(uint8_t y=0;y<TILE_H;++y) {
        const uint16_t *p=frame+(py+y)*WIDTH+px;
        for(uint8_t x=0;x<TILE_W;++x) {
            h^=p[x];
            h*=16777619UL;
        }
    }
    return h;
}
}
bool begin() {
    if (!framebuffer) framebuffer=new(std::nothrow) GFXcanvas16(WIDTH,HEIGHT);
    if (!tx_buffer) tx_buffer=new(std::nothrow) uint16_t[WIDTH*TILE_H];
    if(!framebuffer || !framebuffer->getBuffer() || !tx_buffer) return false;
    framebuffer->setTextWrap(false);
    invalidate();
    return true;
}
void invalidate() { std::memset(valid,0,sizeof(valid)); last_transfer=0U;region_count=0U; }
Adafruit_GFX* canvas() { return framebuffer; }
// [ADD] No extra full-frame copy; transition shifts this frame in place.
uint16_t* pixels() { return framebuffer ? static_cast<uint16_t*>(framebuffer->getBuffer()) : nullptr; }
uint32_t last_bytes() {return last_transfer;}
uint32_t total_bytes() {return total_transfer;}
uint16_t last_regions() {return region_count;}
uint32_t flush(Adafruit_ST7735 &lcd) {
    last_transfer=0U;region_count=0U;
    if(!framebuffer||!framebuffer->getBuffer()||!tx_buffer)return 0U;
    const uint16_t* pixels=(const uint16_t*)framebuffer->getBuffer();
    for(uint8_t row=0;row<TILE_ROWS;++row) {
        bool changed[TILE_COLS]={};
        for(uint8_t col=0;col<TILE_COLS;++col) {
            const uint8_t idx=row*TILE_COLS+col;
            const uint32_t h=hash_tile(pixels,col,row);
            changed[col]=!valid[idx] || previous_hash[idx]!=h;
            previous_hash[idx]=h; valid[idx]=true;
        }
        uint8_t col=0U;
        while(col<TILE_COLS) {
            if(!changed[col]) {++col;continue;}
            const uint8_t start=col;
            while(col<TILE_COLS&&changed[col])++col;
            const uint16_t x=(uint16_t)start*TILE_W;
            const uint16_t width=(uint16_t)(col-start)*TILE_W;
            const uint16_t y=(uint16_t)row*TILE_H;
            for(uint8_t line=0;line<TILE_H;++line) {
                const uint16_t *src=pixels+(y+line)*WIDTH+x;
                std::memcpy(tx_buffer+(uint32_t)line*width,src,(uint32_t)width*2U);
            }
            /* [修改] 使用 Adafruit_SPITFT::drawRGBBitmap 公開 API。
               不再直接將 16 行 RGB565 透過單一 writePixels() 呼叫傳送。
               API 會逐列寫入並負責 RGB565 的 byte order。
               [保留] 8x16 Dirty Tile、相鄰 Tile 合併與傳輸前 hash 比對。 */
            lcd.drawRGBBitmap((int16_t)x, (int16_t)y, tx_buffer,
                              (int16_t)width, (int16_t)TILE_H);
            ++region_count;
            const uint32_t bytes=(uint32_t)width*TILE_H*2U;
            last_transfer+=bytes;
            total_transfer+=bytes;
        }
    }
    return last_transfer;
}
} }
