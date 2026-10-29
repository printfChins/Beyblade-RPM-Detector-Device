/* File: tests/test_style_9.cpp
 * [新增] 3風格畫面確實不同、IR兩組獨立、低電/MAX/HISTORY整頁安全。
 */
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>
#include <Adafruit_GFX.h>
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"
int main() {
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Model m{};m.battery=84;m.voltage_mv=4010;m.measurement.loaded=true;
    m.measurement.value=2500;m.load_adc_valid=m.rpm_adc_valid=true;
    m.load_adc_raw=2000;m.rpm_adc_raw=1600;
    m.history_count=4; m.history[0]=29980;m.history[1]=25100;
    brd_ui::Curve c{};c.count=2;c.peak=2500;c.rpm[0]=2000;c.rpm[1]=2500;
    uint32_t checksums[3]={};
    for(uint8_t style=0;style<3;++style) {
        brd_ui::style_scene::draw(canvas,style,brd_ui::Page::Main,5,m,c);
        const uint16_t *p=(const uint16_t*)canvas.getBuffer();
        uint32_t h=2166136261UL;
        for(unsigned i=0;i<160U*80U;++i){h^=p[i];h*=16777619UL;}
        for(uint8_t j=0;j<style;++j)assert(checksums[j]!=h);
        checksums[style]=h;
        for(brd_ui::Page page:{brd_ui::Page::Max,brd_ui::Page::History,
             brd_ui::Page::Graph,brd_ui::Page::LowBattery,brd_ui::Page::AdcFault})
             brd_ui::style_scene::draw(canvas,style,page,5,m,c);

        // [修改] 兩種警告頁需有獨立的大型圖示及 20 風格的資訊佈局。
        // [移除] 舊版硬編碼電池座標 (33,24) 的錯誤假設。
        const uint8_t originalIds[3]={0,1,5};
        const unsigned legacy=originalIds[style];
        const int pos_x[]={18,51,108,51,18};
        const int pos_y[]={31,31,31,30,32};
        const int ix=pos_x[legacy%5U],iy=pos_y[legacy%5U];
        const auto icon_pixel=[&](int x,int y){
            return ((const uint16_t*)canvas.getBuffer())[y*160+x];
        };
        brd_ui::style_scene::draw(canvas,style,brd_ui::Page::LowBattery,0,m,c);
        // 電池左上框、紅色少量電量，必須與背景明顯不同。
        const uint16_t background=icon_pixel(159,0);
        assert(icon_pixel(ix,iy)!=background);
        assert(icon_pixel(ix+5,iy+6)!=background);
        const auto snapshot=std::vector<uint16_t>(
            (const uint16_t*)canvas.getBuffer(),
            (const uint16_t*)canvas.getBuffer()+160U*80U);
        brd_ui::style_scene::draw(canvas,style,brd_ui::Page::LowBattery,1,m,c);
        unsigned changes=0;
        const auto* now=(const uint16_t*)canvas.getBuffer();
        for(unsigned k=0;k<160U*80U;k++)changes+=(snapshot[k]!=now[k]);
        // [修改] 只有電池最後一格閃爍，不能讓文字/外框一起閃。
        assert(changes==5U*(unsigned)( (legacy%5U==0U?27U:
                legacy%5U==1U?27U:legacy%5U==2U?26U:
                legacy%5U==3U?27U:26U)-10U));
        // 每一個變化圖素都必須位於最後一格電量矩形。
        for(unsigned y=0;y<80U;++y)for(unsigned x=0;x<160U;++x) {
            const unsigned k=y*160U+x;
            if(snapshot[k]!=now[k]) {
                assert(x>=(unsigned)(ix+5) && x<(unsigned)(ix+10));
                assert(y>=(unsigned)(iy+5) && y<(unsigned)(iy+
                    (legacy%5U==0U?27U:legacy%5U==1U?27U:legacy%5U==2U?26U:
                     legacy%5U==3U?27U:26U)-5));
            }
        }
        brd_ui::style_scene::draw(canvas,style,brd_ui::Page::LowBattery,2,m,c);
        const auto* lit=(const uint16_t*)canvas.getBuffer();
        for(unsigned k=0;k<160U*80U;++k)assert(snapshot[k]==lit[k]);
        brd_ui::style_scene::draw(canvas,style,brd_ui::Page::AdcFault,0,m,c);
        assert(icon_pixel(ix+4,iy+3)!=background); // ADC 晶片邊框
        unsigned symbol_differences=0;
        for(int y=iy;y<iy+26;++y)for(int x=ix;x<ix+42;++x){
            symbol_differences+=(icon_pixel(x,y)!=snapshot[y*160+x]);
        }
        assert(symbol_differences>=10U); // 電池外型與帶斷線的 ADC IC 必須可區分。

    }
    m.ir1=false;m.ir2=false;
    brd_ui::style_scene::draw(canvas,0,brd_ui::Page::IrTest,5,m,c);
    uint16_t none_load=((uint16_t*)canvas.getBuffer())[51*160+56];
    m.ir2=true;
    brd_ui::style_scene::draw(canvas,0,brd_ui::Page::IrTest,5,m,c);
    uint16_t load_on=((uint16_t*)canvas.getBuffer())[51*160+56];
    assert(none_load!=load_on);
    m.ir2=false;m.ir1=true;
    brd_ui::style_scene::draw(canvas,0,brd_ui::Page::IrTest,5,m,c);
    uint16_t rpm_only_load=((uint16_t*)canvas.getBuffer())[51*160+56];
    assert(rpm_only_load==none_load);
    std::puts("PASS: 3 distinct main screen signatures, 7 page renders, independent IR highlights.");
}
