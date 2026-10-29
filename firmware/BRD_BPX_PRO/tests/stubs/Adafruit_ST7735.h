/* [測試專用] Adafruit ST7735/GFX API 模擬；正式韌體不編譯此檔。 */
#ifndef TEST_ADAFRUIT_ST7735_H
#define TEST_ADAFRUIT_ST7735_H
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <SPI.h>
#define INITR_MINI160x80 0x04U
#define INITR_MINI160x80_PLUGIN 0x06U
#define ST77XX_BLACK 0x0000U
#define ST77XX_WHITE 0xffffU
#define ST77XX_RED 0xf800U
#define ST77XX_GREEN 0x07e0U
#define ST77XX_BLUE 0x001fU
#define ST77XX_CYAN 0x07ffU
#define ST77XX_YELLOW 0xffe0U
#define ST77XX_MAGENTA 0xf81fU
#define ST77XX_MADCTL 0x36U
#define ST77XX_MADCTL_MY 0x80U
#define ST77XX_MADCTL_MX 0x40U
#define ST77XX_MADCTL_MV 0x20U
#define ST7735_MADCTL_BGR 0x08U
class Adafruit_ST7735 {
public:
    uint16_t pixels[160U*80U] = {};
    uint32_t init_count=0;
    uint32_t transfer_count=0;
    uint32_t transferred_bytes=0;
    int16_t window_x=0,window_y=0,window_w=0,window_h=0;
    uint32_t full_clear_count=0;
    uint32_t madctl_write_count=0;
    uint8_t last_madctl=0;
    uint64_t drawn_pixels=0;
    Adafruit_ST7735(SPIClass *, int16_t cs, int16_t dc, int16_t rst) {
        assert(cs == -1 && dc == 2 && rst == -1);
    }
    void initR(uint8_t mode) {
        assert(mode == INITR_MINI160x80_PLUGIN || mode == INITR_MINI160x80); initialized_ = true; ++init_count;
    }
    // [修改][測試替身] 支援此專案目前使用的 16 MHz SPI 設定。
    void setSPISpeed(uint32_t hz) { assert(initialized_ && (hz == 4000000UL || hz == 8000000UL || hz == 16000000UL)); }
    void setRotation(uint8_t value) { assert(value == 1U); }
    void invertDisplay(bool value) { (void)value; }
    void sendCommand(uint8_t command, const uint8_t *data, uint8_t count) {
        assert(command == ST77XX_MADCTL && data != nullptr && count == 1U);
        ++madctl_write_count;
        last_madctl = data[0];
    }
    void setTextWrap(bool) {}
    int16_t width() const { return 160; }
    int16_t height() const { return 80; }
    void startWrite() {}
    void endWrite() {}
    void setAddrWindow(uint16_t x,uint16_t y,uint16_t w,uint16_t h) {
        window_x=x;window_y=y;window_w=w;window_h=h;
        assert(x+w<=160 && y+h<=80);
    }
    void writePixels(uint16_t *data,uint32_t length,bool block=true,bool bigEndian=false) {
        (void)block;(void)bigEndian;
        assert(length==(uint32_t)window_w*(uint32_t)window_h);
        for(int32_t row=0;row<window_h;row++)
            for(int32_t col=0;col<window_w;col++)
                pixels[(window_y+row)*160+window_x+col]=data[row*window_w+col];
        ++transfer_count;transferred_bytes+=length*2U;
    }
    // [新增] 模擬 Adafruit_SPITFT::drawRGBBitmap，驗證 tile 座標與內容。
    void drawRGBBitmap(int16_t x,int16_t y,uint16_t *pcolors,int16_t w,int16_t h) {
        assert(x>=0 && y>=0 && w>0 && h>0 && x+w<=160 && y+h<=80);
        for(int16_t row=0;row<h;++row)
            for(int16_t col=0;col<w;++col)
                pixels[(y+row)*160+x+col]=pcolors[row*w+col];
        ++transfer_count;
        transferred_bytes+=(uint32_t)w*h*2U;
    }
    void setTextSize(uint8_t scale) { size_ = scale; }
    void setTextColor(uint16_t color) { text_color_ = color; text_background_enabled_ = false; }
    void setTextColor(uint16_t color, uint16_t background) { text_color_ = color; text_background_ = background; text_background_enabled_ = true; }
    void setCursor(int16_t x, int16_t y) { cursor_x_=x;cursor_y_=y; }
    void drawPixel(int16_t x,int16_t y,uint16_t color) {
        if(x>=0 && x<160 && y>=0 && y<80) { pixels[y*160+x]=color; ++drawn_pixels; }
    }
    void fillScreen(uint16_t c) { ++full_clear_count; drawn_pixels += 160U*80U; for(auto &p:pixels){p=c;} }
    void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c) {
        if(w<=0||h<=0)return;
        for(int32_t yy=y;yy<(int32_t)y+h;yy++) for(int32_t xx=x;xx<(int32_t)x+w;xx++)
            drawPixel(xx,yy,c);
    }
    void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c) {fillRect(x,y,w,1,c);}
    void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c) {fillRect(x,y,1,h,c);}
    void drawRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c) {
        drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);
        drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);
    }
    void drawLine(int16_t x0,int16_t y0,int16_t x1,int16_t y1,uint16_t c) {
        int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
        while(true) {
            drawPixel(x0,y0,c);
            if(x0==x1&&y0==y1)break;
            int e2=2*err;
            if(e2>=dy){err+=dy;x0+=sx;}
            if(e2<=dx){err+=dx;y0+=sy;}
        }
    }
    void drawCircle(int16_t cx,int16_t cy,int16_t r,uint16_t c) {
        int x=r,y=0,err=1-r;
        while(x>=y){
            drawPixel(cx+x,cy+y,c);drawPixel(cx+y,cy+x,c);
            drawPixel(cx-y,cy+x,c);drawPixel(cx-x,cy+y,c);
            drawPixel(cx-x,cy-y,c);drawPixel(cx-y,cy-x,c);
            drawPixel(cx+y,cy-x,c);drawPixel(cx+x,cy-y,c);
            y++;
            if(err<0){err+=2*y+1;}else{x--;err+=2*(y-x+1);}
        }
    }
    void fillCircle(int16_t cx,int16_t cy,int16_t r,uint16_t c) {
        for(int yy=-r;yy<=r;yy++)for(int xx=-r;xx<=r;xx++)
            if(xx*xx+yy*yy<=r*r)drawPixel(cx+xx,cy+yy,c);
    }
    void fillTriangle(int16_t ax,int16_t ay,int16_t bx,int16_t by,int16_t cx,int16_t cy,uint16_t c) {
        int minx=ax<bx?ax:bx,maxx=ax>bx?ax:bx,miny=ay<by?ay:by,maxy=ay>by?ay:by;
        if(cx<minx)minx=cx;
        if(cx>maxx)maxx=cx;
        if(cy<miny)miny=cy;
        if(cy>maxy)maxy=cy;
        const auto cross=[](int x0,int y0,int x1,int y1,int x,int y){
            return (x1-x0)*(y-y0)-(y1-y0)*(x-x0);
        };
        const int orient=cross(ax,ay,bx,by,cx,cy);
        for(int yy=miny;yy<=maxy;yy++)for(int xx=minx;xx<=maxx;xx++){
            int a=cross(ax,ay,bx,by,xx,yy),b=cross(bx,by,cx,cy,xx,yy),d=cross(cx,cy,ax,ay,xx,yy);
            if(orient>=0?(a>=0&&b>=0&&d>=0):(a<=0&&b<=0&&d<=0))drawPixel(xx,yy,c);
        }
    }
    void print(const char *str) {
        for(const char *p=str;*p;p++,cursor_x_+=(int16_t)6*size_){
            const uint8_t *glyph=font(*p);
            if (text_background_enabled_) fillRect(cursor_x_, cursor_y_, 6*size_, 8*size_, text_background_);
            for(int col=0;col<5;col++)for(int row=0;row<7;row++){
                if((glyph[col]&(1U<<row))!=0U)
                    fillRect(cursor_x_+col*size_,cursor_y_+row*size_,size_,size_,text_color_);
            }
        }
    }
private:
    bool initialized_=false;
    uint8_t size_=1;
    uint16_t text_color_=ST77XX_WHITE;
    uint16_t text_background_=ST77XX_BLACK;
    bool text_background_enabled_=false;
    int16_t cursor_x_=0,cursor_y_=0;
    static const uint8_t *font(char c) {
        static const uint8_t blank[5]={0,0,0,0,0};
        static const uint8_t glyph_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        static const uint8_t glyph_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
        static const uint8_t glyph_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
        static const uint8_t glyph_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
        static const uint8_t glyph_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
        static const uint8_t glyph_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
        static const uint8_t glyph_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
        static const uint8_t glyph_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
        static const uint8_t glyph_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
        static const uint8_t glyph_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
        static const uint8_t glyph_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
        static const uint8_t glyph_A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
        static const uint8_t glyph_B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
        static const uint8_t glyph_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
        static const uint8_t glyph_D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
        static const uint8_t glyph_E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
        static const uint8_t glyph_G[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
        static const uint8_t glyph_K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
        static const uint8_t glyph_H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
        static const uint8_t glyph_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
        static const uint8_t glyph_L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
        static const uint8_t glyph_M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
        static const uint8_t glyph_N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
        static const uint8_t glyph_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
        static const uint8_t glyph_P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
        static const uint8_t glyph_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
        static const uint8_t glyph_S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
        static const uint8_t glyph_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
        static const uint8_t glyph_U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
        static const uint8_t glyph_V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
        static const uint8_t glyph_W[5] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
        static const uint8_t glyph_X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
        static const uint8_t glyph_Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
        static const uint8_t glyph_dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
        static const uint8_t glyph_percent[5] = {0x63, 0x13, 0x08, 0x64, 0x63};
        static const uint8_t glyph_underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
        static const uint8_t glyph_F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
        switch(c) {
            case ' ': return glyph_space;
            case '0': return glyph_0;
            case '1': return glyph_1;
            case '2': return glyph_2;
            case '3': return glyph_3;
            case '4': return glyph_4;
            case '5': return glyph_5;
            case '6': return glyph_6;
            case '7': return glyph_7;
            case '8': return glyph_8;
            case '9': return glyph_9;
            case 'A': return glyph_A;
            case 'B': return glyph_B;
            case 'C': return glyph_C;
            case 'D': return glyph_D;
            case 'E': return glyph_E;
            case 'G': return glyph_G;
            case 'K': return glyph_K;
            case 'H': return glyph_H;
            case 'I': return glyph_I;
            case 'L': return glyph_L;
            case 'M': return glyph_M;
            case 'N': return glyph_N;
            case 'O': return glyph_O;
            case 'P': return glyph_P;
            case 'R': return glyph_R;
            case 'S': return glyph_S;
            case 'T': return glyph_T;
            case 'U': return glyph_U;
            case 'V': return glyph_V;
            case 'W': return glyph_W;
            case 'X': return glyph_X;
            case 'Y': return glyph_Y;
            case '.': return glyph_dot;
            case '%': return glyph_percent;
            case '_': return glyph_underscore;
            case 'F': return glyph_F;
            default: return blank;
        }
    }
};
#endif
