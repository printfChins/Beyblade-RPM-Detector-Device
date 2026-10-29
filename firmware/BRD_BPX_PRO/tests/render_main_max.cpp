/*
 * File: BRD_BPX_PRO/tests/render_main_max.cpp
 * [修改] 主機端 TORQUE MAIN 即時/MAX SP 切換回歸與 RGB565 預覽輸出。
 * [注意] 主機端工具，不在 ESP32-C3 執行。
 */
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "brd_ui_navigation.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"

static void save(const char* folder,unsigned theme,const char* mode,const uint16_t* pixels) {
    char path[512];
    std::snprintf(path,sizeof path,"%s/%02u_%s.rgb565",folder,theme,mode);
    FILE* f=std::fopen(path,"wb");
    if(!f){std::perror(path);std::exit(2);}
    if(std::fwrite(pixels,sizeof(uint16_t),160U*80U,f)!=160U*80U){std::exit(3);}
    std::fclose(f);
}

int main(int argc,char**argv) {
    if(argc!=2)return 2;
    brd_ui::Model model{};
    model.measurement.loaded=true;
    model.measurement.value=2500U;
    model.connected=true;
    model.battery=84U;
    model.voltage_mv=4100U;
    brd_ui::Curve curve{};
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    for(unsigned theme=0;theme<1U;++theme) {
        model.measurement.show_max=false;
        model.measurement.value=2500U;
        assert(brd_ui::choose_display_page(model)==brd_ui::Page::Main);
        brd_ui::style_scene::draw(canvas,(uint8_t)theme,brd_ui::Page::Main,5U,model,curve);
        const auto* raw_live=static_cast<const uint16_t*>(canvas.getBuffer());
        const std::vector<uint16_t> live(raw_live,raw_live+160U*80U);
        save(argv[1],theme,"LIVE",live.data());
        model.measurement.show_max=true;
        model.measurement.value=29980U;
        assert(brd_ui::choose_display_page(model)==brd_ui::Page::Main);
        brd_ui::style_scene::draw(canvas,(uint8_t)theme,brd_ui::Page::Main,5U,model,curve);
        const auto* raw_peak=static_cast<const uint16_t*>(canvas.getBuffer());
        const std::vector<uint16_t> peak(raw_peak,raw_peak+160U*80U);
        save(argv[1],theme,"MAX_SP",peak.data());
        // [新增] MAX SP 的 0..8 幀供 GIF 預覽，與正式 style_scene 相同的繪圖入口。
        for(unsigned frame=0U;frame<9U;++frame) {
            brd_ui::style_scene::draw(canvas,(uint8_t)theme,brd_ui::Page::Main,
                                      (uint8_t)frame,model,curve);
            char frame_mode[32];
            std::snprintf(frame_mode,sizeof frame_mode,"MAX_F%02u",frame);
            save(argv[1],theme,frame_mode,
                 static_cast<const uint16_t*>(canvas.getBuffer()));
        }
        assert(live!=peak);
        // BLE/battery slot unchanged by live/max switch.
        for(int y=0;y<17;++y)for(int x=90;x<160;++x) {
            assert(live[(size_t)y*160U+(size_t)x]==peak[(size_t)y*160U+(size_t)x]);
        }
        model.measurement.show_max=false;
        model.measurement.value=2500U;
        brd_ui::style_scene::draw(canvas,(uint8_t)theme,brd_ui::Page::Main,5U,model,curve);
        assert(std::memcmp(canvas.getBuffer(),live.data(),live.size()*sizeof(uint16_t))==0);
    }
    std::puts("PASS: TORQUE MAIN retains its background, show MAX SP in-place, preserve BLE/battery and restore LIVE");
    return 0;
}
