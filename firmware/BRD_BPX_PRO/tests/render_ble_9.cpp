/* BRD_BPX_PRO/tests/render_ble_9.cpp
 * [新增] Host-only preview; uses production C++ scene and GFXcanvas16.
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
int main(int argc,char**argv){
    if(argc!=2)return 2;
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Model m{};
    m.measurement.value=2500;
    m.measurement.loaded=true;
    m.battery=84;
    m.voltage_mv=4100;
    brd_ui::Curve c{};
    const char* labels[]={"connected","waiting","disabled"};
    for(unsigned i=0;i<9;++i)for(unsigned mode=0;mode<3;++mode){
        m.connected=(mode==0);
        m.ble_disabled=(mode==2);
        brd_ui::style_scene::draw(canvas,(uint8_t)i,brd_ui::Page::Main,4,m,c);
        char dest[500];
        std::snprintf(dest,sizeof dest,"%s/style%02u_%s.rgb565",argv[1],i,labels[mode]);
        FILE *fp=std::fopen(dest,"wb");if(!fp)return 3;
        if(std::fwrite(canvas.getBuffer(),sizeof(uint16_t),160U*80U,fp)!=160U*80U) return 4;
        std::fclose(fp);
    }
    return 0;
}
