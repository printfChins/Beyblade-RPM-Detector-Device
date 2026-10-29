/*
 * File: BRD_BPX_PRO/brd_ui_warning_pages.cpp
 * [修改] 只保留 TORQUE 低電／ADC 警告頁構圖，像素位置不變。
 * [保留] 使用原生 Adafruit_GFX 繪圖 API；不建立自製 TFT 驅動。
 * [設計] LOW BATTERY 使用電池最後一格紅色電量持續閃爍；ADC FAULT 必須有
 *        帶斷線記號的感測器 IC，兩者均具有清楚的文字提示。
 * [效能] 畫面交給 brd_ui_dirty_presenter 做 8x16 Tile 差異比對。
 */
#include "brd_ui_warning_pages.h"
#include "brd_ui_legible_text.h" // [新增] 警告頁次要字體改用 U8g2。
#include <cstring>
namespace brd_ui { namespace warning_pages {
namespace {
struct Rect { int x,y,w,h; };

static void h(Adafruit_GFX& g, int x, int y, int w,uint16_t c) {g.drawFastHLine(x,y,w,c);}
static void line(Adafruit_GFX& g,int x,int y,int xx,int yy,uint16_t c) {g.drawLine(x,y,xx,yy,c);}
static void rect(Adafruit_GFX& g,int x,int y,int w,int ht,uint16_t c) {g.drawRect(x,y,w,ht,c);}
static void fill(Adafruit_GFX& g,int x,int y,int w,int ht,uint16_t c) {g.fillRect(x,y,w,ht,c);}
static void label(Adafruit_GFX&g,int x,int y,const char *s,uint16_t c,uint8_t scale=1U) {
    // [修改] 所有 scale=1 文字不再使用 6x8，避免警告說明過小。
    if(scale==1U){legible_text::draw(g,x,y,s,c);return;}
    g.setTextWrap(false);
    g.setTextSize(scale);
    g.setTextColor(c);
    g.setCursor(x,y);
    g.print(s);
}
static uint16_t subdued(uint16_t a,uint16_t b) {
    // [新增] 低亮度主題線條：55% 背景、45% muted。
    int ar=(a>>11)&31,ag=(a>>5)&63,ab=a&31;
    int br=(b>>11)&31,bg=(b>>5)&63,bb=b&31;
    return (uint16_t)(((ar*11+br*9)/20<<11)|((ag*11+bg*9)/20<<5)|((ab*11+bb*9)/20));
}
static void motifs(Adafruit_GFX &g, const Palette &p) {
    const uint16_t m=subdued(p.background,p.muted);
    // [保留] TORQUE 原始 style=5：左側深色實心面板與水平工業格線。
    fill(g,0,0,53,80,p.muted);
    for(int y=6;y<73;y+=7)h(g,0,y,49,m);
}
static Rect iconBounds() {
    // [保留] TORQUE 原 style=5 % 5 == 0 的電池 / ADC 圖示座標。
    return {18,31,43,27};
}
static void drawBattery(Adafruit_GFX& g,const Rect&r,const Palette&p,uint8_t frame) {
    const uint16_t outline=p.foreground;
    // [保留] TORQUE 電池輪廓、凸極與原有配色。
    rect(g,r.x,r.y,r.w-5,r.h,outline);
    fill(g,r.x+r.w-5,r.y+r.h/3,5,r.h/3,outline);

    // [刪除] 電池內原本的驚嘆號與電池外額外的小燈。
    // [新增] 最後一格電量紅色閃爍（500 ms 亮 / 500 ms 滅）。
    // [效能] 紅色格以外的圖素完全固定，由 Dirty Tile 限縮 SPI 寫入。
    if((frame&1U)==0U) {
        fill(g,r.x+5,r.y+5,5,r.h-10,p.danger);
    }
}
static void drawAdc(Adafruit_GFX& g,const Rect&r,const Palette&p,uint8_t frame) {
    const uint16_t out=p.foreground;
    const int cx=r.x+4, cy=r.y+3, cw=r.w-11,ch=r.h-6;
    rect(g,cx,cy,cw,ch,out);
    // [新增] 兩側端子：一看便知道是感測/電路故障，不會與電池混淆。
    for(int i=0;i<3;i++){
        int yy=cy+5+i*(ch-8)/2;
        h(g,r.x,yy,4,p.accent);
        h(g,cx+cw,yy,4,p.accent);
    }
    // 斷線的 X 及警告中心符號。
    line(g,cx+cw/2-6,cy+5,cx+cw/2+6,cy+ch-5,p.warning);
    line(g,cx+cw/2+6,cy+5,cx+cw/2-6,cy+ch-5,p.warning);
    g.drawPixel(cx+cw/2,cy+ch/2,(frame&1U)?p.danger:p.warning);
}
static void message(Adafruit_GFX&g,Kind kind,const Palette&p) {
    const bool low=(kind==Kind::LowBattery);
    // [保留] TORQUE 原 style=5 % 5 == 0 的文字與字型大小。
    label(g,72,25,low?"LOW":"ADC",p.foreground,2);
    label(g,72,43,low?"BATT":"FAULT",p.danger,2);
    label(g,72,65,low?"CHARGE NOW":"CHECK INPUT",p.accent);
}
}
void draw(Adafruit_GFX&gfx,uint8_t theme,Kind kind,uint8_t frame,const Palette&palette){
    // [保留] Dirty Presenter 比較整張 RGB565 畫布，實際 LCD 只更新有變化的 Tiles。
    gfx.fillScreen(palette.background);
    (void)theme; // [修改] 舊參數保留相容；警示畫面只使用 TORQUE。
    motifs(gfx,palette);
    // [保留] TORQUE 原 style=5 % 5 == 0 文字清理區。
    fill(gfx,70,22,90,58,palette.background);
    const Rect r=iconBounds();
    // [新增] 圖示背景清理，避免風格網格穿過電池或故障符號。
    fill(gfx,r.x-3,r.y-3,r.w+7,r.h+7,palette.background);
    if(kind==Kind::LowBattery)drawBattery(gfx,r,palette,frame);
    else drawAdc(gfx,r,palette,frame);
    message(gfx,kind,palette);
}
} }
