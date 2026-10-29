/*
 * File: BRD_BPX_PRO/tests/test_legible_text.cpp
 * [修改] 驗證 U8g2 字型選取與原有 Adafruit GFX 畫布整合。
 * [注意] 主機端 U8g2 替身只驗證介面與邊界，不代表真實字型外觀。
 */
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include "brd_ui_legible_text.h"
namespace lt=brd_ui::legible_text;

static unsigned count(const GFXcanvas16 &canvas) {
    unsigned n=0;
    for(uint16_t c: canvas.pixels) n+=(c!=0U);
    return n;
}
int main() {
    GFXcanvas16 screen(160,80);
    const auto w=[](GFXcanvas16 &g,const char *text,lt::Density d=lt::Density::Regular){
        return lt::width(g,text,d);
    };
    assert(w(screen,"WAIT LOAD")==63);
    assert(w(screen,"100%")==28);
    assert(w(screen,"4.10V")==35);
    assert(w(screen,"29980",lt::Density::Compact)==30);
    assert(127+w(screen,"100%")<=160);
    assert(118+w(screen,"4.10V")<=160);
    assert(127+w(screen,"4095")<=160);
    assert(111+w(screen,"MAX SP")<=160);
    assert(116+w(screen,"29980",lt::Density::Compact)<=160);
    screen.fillScreen(0U);
    lt::draw(screen,118,3,"4.10V",0xffffU);
    assert(count(screen)>0U);
    screen.fillScreen(0U);
    lt::draw(screen,116,66,"29980",0xffffU,lt::Density::Compact);
    assert(count(screen)>0U);
    screen.fillScreen(0U);
    lt::draw(screen,15,15,"NO CUSTOM GLYPHS",0xffffU);
    assert(count(screen)>0U);
    std::puts("PASS: U8g2 real font selection, Adafruit GFX adapter and safe layout (host API stub)");
}
