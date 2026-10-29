#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
int main(int argc,char** argv) {
    if(argc != 2) return 2;
    brd_ui::Model m{};
    m.battery=84;
    m.voltage_mv=4100;
    m.charging=false;
    m.connected=true;
    brd_ui::Curve c{};
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    for(int style=0;style<3;++style){
        for(int f=0;f<8;++f){
            brd_ui::style_scene::draw(canvas,(uint8_t)style,brd_ui::Page::Boot,(uint8_t)f,m,c);
            char path[512];
            std::snprintf(path,sizeof path,"%s/style%02d_boot_f%02d.rgb565",argv[1],style,f);
            FILE* fb=std::fopen(path,"wb"); if(!fb)return 3;
            std::fwrite(canvas.getBuffer(),sizeof(uint16_t),160U*80U,fb); std::fclose(fb);
        }
        const uint8_t percents[] = {0U,25U,50U,75U,100U};
        const bool verifies[] = {false,false,false,true,false};
        const uint8_t phases[] = {0U,0U,0U,0U,(uint8_t)brd_ota::COMPLETE};
        for(int i=0;i<5;++i){
            m.ota.active=true;
            m.ota.percent=percents[i];
            m.ota.verifying=verifies[i];
            m.ota.phase=(brd_ota::Phase)phases[i];
            brd_ui::style_scene::draw(canvas,(uint8_t)style,brd_ui::Page::Ota,(uint8_t)(i+1),m,c);
            char path[512];
            std::snprintf(path,sizeof path,"%s/style%02d_ota_s%02d.rgb565",argv[1],style,i);
            FILE* fo=std::fopen(path,"wb"); if(!fo)return 4;
            std::fwrite(canvas.getBuffer(),sizeof(uint16_t),160U*80U,fo); std::fclose(fo);
        }
    }
    return 0;
}
