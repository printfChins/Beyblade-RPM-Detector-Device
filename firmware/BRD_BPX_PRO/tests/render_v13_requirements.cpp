/* [新增] 主機端用正式 C++ TORQUE 繪圖函式輸出 RGB565 截圖，非 AI 生圖。 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstring>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
static bool save(const char*folder,const char*name,Adafruit_GFX&gfx) {
    char path[512];snprintf(path,sizeof(path),"%s/%s.rgb565",folder,name);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(static_cast<GFXcanvas16&>(gfx).getBuffer(),sizeof(uint16_t),12800U,f)==12800U;
    fclose(f);return ok;
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    GFXcanvas16 cv(160,80);cv.setTextWrap(false);
    brd_ui::Model m{};m.battery=84U;m.voltage_mv=4040U;m.connected=true;
    brd_ui::Curve c{};c.count=32;c.peak=29980;
    for(int i=0;i<32;i++)c.rpm[i]=static_cast<uint16_t>(9000+12000*(i%11)/10);
    const char *names[]={"BOOT_0","BOOT_8","WAIT","LOADED","LIVE","MAX","GRAPH","HISTORY","OTA"};
    for(int i=0;i<9;i++) {
        m.measurement.loaded=(i==3||i==4);
        m.measurement.show_max=(i==5);
        m.measurement.value=(i==4?15800:(i==5?29980:0));
        m.measurement.peak_value=m.measurement.value;
        m.history_count=8;m.history_peak=29980;
        const uint16_t history[8]={29980,26100,25000,23870,21120,19800,17680,15000};
        for(int q=0;q<8;q++)m.history[q]=history[q];
        m.ota.percent=56U;
        brd_ui::Page page=brd_ui::Page::Main;
        unsigned frame=12U;
        if(i<=1){page=brd_ui::Page::Boot;frame=(i==0?0:16);}
        if(i==2){page=brd_ui::Page::Main;frame=16;}
        if(i==3){page=brd_ui::Page::Main;frame=7;}
        if(i==6){page=brd_ui::Page::Graph;frame=20;}
        if(i==7){page=brd_ui::Page::History;frame=20;}
        if(i==8){page=brd_ui::Page::Ota;frame=0;}
        brd_ui::style_scene::draw(cv,0U,page,static_cast<uint8_t>(frame),m,c);
        if(!save(argv[1],names[i],cv))return 3;
    }
    puts("PASS: render production TORQUE V1.3 9 frames (RGB565).");
}
