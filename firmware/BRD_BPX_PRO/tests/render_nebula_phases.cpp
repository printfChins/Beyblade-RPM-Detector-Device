/*
 * File: BRD_BPX_PRO/tests/render_nebula_phases.cpp
 * [新增] 主機端同源 UI 動畫 RGB565 影格輸出（不編入 Arduino 韌體）。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"

int main(int argc,char **argv) {
    if(argc!=2) return 2;
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Model model{};
    brd_ui::Curve curve{};
    model.battery=84;
    model.voltage_mv=4090;
    model.connected=true;
    for(unsigned frame=0;frame<50U;++frame) {
        model.measurement.show_max=false;
        model.measurement.loaded=false;
        model.measurement.value=0U;
        model.measurement.peak_value=0U;
        uint8_t anim_tick=(uint8_t)frame;
        if(frame>=6U && frame<15U) {
            model.measurement.loaded=true;
        } else if(frame>=15U && frame<31U) {
            const uint32_t peak=2000U+(frame-15U)*1700U;
            model.measurement.peak_value=(uint16_t)peak;
            model.measurement.value=(uint16_t)peak;
        } else if(frame>=31U && frame<37U) {
            model.measurement.peak_value=27500U;
            model.measurement.value=(uint16_t)(27500U-(frame-31U)*4500U);
        } else if(frame>=37U && frame<45U) {
            model.measurement.show_max=true;
            model.measurement.peak_value=27500U;
            model.measurement.value=27500U;
        }
        for(unsigned theme=0;theme<3U;++theme) {
            brd_ui::style_scene::draw(canvas,(uint8_t)theme,brd_ui::Page::Main,anim_tick,model,curve);
            char name[512];
            std::snprintf(name,sizeof name,"%s/theme%u_frame%02u.rgb565",argv[1],theme,frame);
            FILE*f=std::fopen(name,"wb");
            if(!f)return 3;
            if(std::fwrite(canvas.getBuffer(),sizeof(uint16_t),160U*80U,f)!=160U*80U)return 4;
            std::fclose(f);
        }
    }
    return 0;
}
