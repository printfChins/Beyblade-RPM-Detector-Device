/*
 * File: BRD_BPX_PRO/tests/render_indicator_20k.cpp
 * [新增] 從正式三風格 GFX 場景產生待機、裝載、LIVE 和 MAX 影格。
 * 本檔僅作主機端 160x80 預覽，不在 Arduino 中編譯。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "brd_config.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Curve curve{};
    for(unsigned frame=0U;frame<46U;++frame) {
        brd_ui::Model m{};
        m.battery=85U;
        m.connected=true;
        m.voltage_mv=4010U;
        m.measurement.loaded=(frame>=7U && frame<18U);
        uint32_t rpm=0U;
        if(frame>=18U && frame<40U) {
            rpm=1200U+(frame-18U)*1100U;
        } else if(frame>=40U) {
            rpm=24000U;
            m.measurement.show_max=true;
        }
        m.measurement.value=rpm;
        m.measurement.peak_value=rpm;
        for(unsigned theme=0U;theme<3U;++theme) {
            brd_ui::style_scene::draw(canvas,static_cast<uint8_t>(theme),
                brd_ui::Page::Main,static_cast<uint8_t>(frame),m,curve);
            char path[512];
            std::snprintf(path,sizeof(path),"%s/theme%u_frame%02u.rgb565",argv[1],theme,frame);
            FILE *fp=std::fopen(path,"wb");
            if(!fp)return 3;
            const size_t size=std::fwrite(canvas.getBuffer(),sizeof(uint16_t),
                                         160U*80U,fp);
            std::fclose(fp);
            if(size!=160U*80U)return 4;
        }
    }
    return 0;
}
