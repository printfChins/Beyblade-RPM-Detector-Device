/*
 * File: BRD_BPX_PRO/tests/render_selected_9.cpp
 * [新增] 僅主機端：利用正式 style_scene C++ + Adafruit GFX mock 輸出九套七頁 RGB565 像素。
 * [注意] 這是預覽工具，ESP32-C3 Arduino 韌體不會編譯本檔。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
int main(int argc,char** argv) {
    if(argc != 2) return 2;
    const brd_ui::Page pages[] = {brd_ui::Page::Main, brd_ui::Page::Max,
        brd_ui::Page::Graph, brd_ui::Page::History, brd_ui::Page::IrTest,
        brd_ui::Page::LowBattery, brd_ui::Page::AdcFault};
    brd_ui::Model m{};
    m.measurement.value=2500;
    m.measurement.loaded=true;
    m.battery=84;
    m.voltage_mv=4100;
    m.connected=true;
    m.load_adc_valid=true;
    m.rpm_adc_valid=true;
    m.load_adc_raw=1780;
    m.rpm_adc_raw=2870;
    m.ir1=true;
    m.ir2=true;
    m.history_count=4;
    m.history[0]=29980;
    m.history[1]=28240;
    m.history[2]=25100;
    m.history[3]=22810;
    brd_ui::Curve c{};
    c.count=32;
    c.peak=29980;
    for(unsigned i=0;i<c.count;++i) c.rpm[i]=(uint16_t)(i<7 ? i*4282 : 29980U-(i-7U)*1120U);
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    for(int style=0;style<3;++style){
        for(int page=0;page<7;++page){
            m.measurement.value=(page==1)?29980:2500;
            brd_ui::style_scene::draw(canvas,(uint8_t)style,pages[page],
                (uint8_t)(page==5?0:14),m,c);
            char path[512];
            std::snprintf(path,sizeof path,"%s/style%02d_page%d.rgb565",argv[1],style,page);
            FILE* f=std::fopen(path,"wb");
            if(!f)return 3;
            if(std::fwrite(canvas.getBuffer(),sizeof(uint16_t),160U*80U,f)!=160U*80U)return 4;
            std::fclose(f);
        }
    }
    return 0;
}
