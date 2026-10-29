/* 測試替身：正式韌體只使用官方 Adafruit_GFX.h。 */
#ifndef TEST_ADAFRUIT_GFX_H
#define TEST_ADAFRUIT_GFX_H
#include <Adafruit_ST7735.h>
using Adafruit_GFX=Adafruit_ST7735;
class GFXcanvas16 : public Adafruit_ST7735 {
public:
    GFXcanvas16(uint16_t w,uint16_t h) : Adafruit_ST7735(&SPI,-1,2,-1) {assert(w==160 && h==80);}
    void* getBuffer() {return pixels;}
};
#endif
