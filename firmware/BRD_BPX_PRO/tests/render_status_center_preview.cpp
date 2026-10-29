/*
 * File: BRD_BPX_PRO/tests/render_status_center_preview.cpp
 * [新增] 從正式 brd_ui_style_scene::draw() 輸出三風格 WAIT LOAD / LOADED 的 RGB565 畫面。
 * [用途] 主機端預覽，不編入 ESP32-C3 韌體。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"

int main(int argc,char **argv) {
    if(argc!=2) return 2;
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Curve curve{};
    for(int frame=0;frame<32;++frame) {
        for(int style=0;style<3;++style) {
            for(int state=0;state<2;++state) {
                brd_ui::Model model{};
                model.battery=84;
                model.voltage_mv=4090;
                model.connected=true;
                model.measurement.loaded=(state==1);
                model.measurement.value=0;
                model.measurement.peak_value=0;
                const uint8_t tick=static_cast<uint8_t>(state==0 ? (frame*3/2)%48 : (frame<8?frame:8));
                brd_ui::style_scene::draw(canvas,static_cast<uint8_t>(style),
                                          brd_ui::Page::Main,tick,model,curve);
                char path[600];
                std::snprintf(path,sizeof(path),"%s/f%02d_s%d_q%d.rgb565",argv[1],frame,style,state);
                FILE *out=std::fopen(path,"wb");
                if(!out) return 3;
                if(std::fwrite(canvas.getBuffer(),sizeof(uint16_t),12800,out)!=12800) {
                    std::fclose(out);return 4;
                }
                std::fclose(out);
            }
        }
    }
    return 0;
}
