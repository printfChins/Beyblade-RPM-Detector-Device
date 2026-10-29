/*
 * File: BRD_BPX_PRO/tests/render_legible_3themes.cpp
 * [新增] 輸出完整 3 風格 160x80 C++ 場景用於實際字體大小審查。
 */
#include <Adafruit_GFX.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"
#include "brd_ui_types.h"

int main(int argc,char** argv) {
    if (argc!=2) return 2;
    brd_ui::Curve curve{};
    curve.count=32U;curve.peak=29980U;
    for(unsigned i=0;i<curve.count;++i) {
        curve.rpm[i]=static_cast<uint16_t>(i<7U?i*4282U:29980U-(i-7U)*1120U);
    }
    GFXcanvas16 cv(160U,80U);cv.setTextWrap(false);
    for(uint8_t theme=0U;theme<3U;++theme) {
        for(unsigned page=0U;page<12U;++page) {
            brd_ui::Model m{};
            m.battery=85U;m.connected=true;m.voltage_mv=4120U;
            m.load_adc_raw=1780U;m.rpm_adc_raw=2870U;
            m.ir1=true;m.ir2=true;
            m.history_count=4;
            m.history[0]=29980U;m.history[1]=28240U;
            m.history[2]=25100U;m.history[3]=22810U;
            brd_ui::Page p=brd_ui::Page::Main;
            switch(page) {
                case 0: m.measurement.loaded=false;break;
                case 1: m.measurement.loaded=true;break;
                case 2: m.measurement.value=18700U;m.measurement.peak_value=18700U;break;
                case 3: m.measurement.value=24000U;m.measurement.peak_value=24000U;m.measurement.show_max=true;break;
                case 4: p=brd_ui::Page::Graph;break;
                case 5: p=brd_ui::Page::History;break;
                case 6: p=brd_ui::Page::IrTest;break;
                case 7: p=brd_ui::Page::LowBattery;break;
                case 8: p=brd_ui::Page::AdcFault;break;
                case 9: p=brd_ui::Page::Boot;break;
                case 10:p=brd_ui::Page::Ota;m.ota.percent=50U;break;
                case 11:p=brd_ui::Page::Ota;m.ota.percent=80U;m.ota.verifying=true;break;
            }
            brd_ui::style_scene::draw(cv,theme,p,9U,m,curve);
            char name[400];
            std::snprintf(name,sizeof(name),"%s/theme%u_page%02u.rgb565",argv[1],theme,page);
            FILE*f=std::fopen(name,"wb");if(!f)return 3;
            const size_t n=std::fwrite(cv.getBuffer(),sizeof(uint16_t),160U*80U,f);
            std::fclose(f);if(n!=160U*80U)return 4;
        }
    }
    return 0;
}
