/*
 * File: BRD_BPX_PRO/tests/export_transition_frames.cpp
 * [ADD] Host-only. Calls the SAME brd_ui_transition.cpp as BRD firmware.
 *       Exports the actual 160x80 RGB565 framebuffer after every slide step.
 */
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <cstdio>
#include <cstring>
#include "brd_ui_screen.h"
#include "brd_ui_transition.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_dirty_presenter.h"
namespace brd_ui {
namespace { Adafruit_ST7735 display(&SPI,-1,2,-1); }
Adafruit_ST7735 &lcd(){return display;}
}
static bool export_frame(const char*dir,unsigned style,unsigned page,unsigned step){
    char path[512];
    std::snprintf(path,sizeof(path),"%s/style%u_page%u_step%u.rgb565",
                  dir,style,page,step);
    FILE *f=std::fopen(path,"wb");
    if(!f)return false;
    const size_t n=std::fwrite(brd_ui::dirty_presenter::pixels(),
                               sizeof(uint16_t),160U*80U,f);
    std::fclose(f);
    return n==160U*80U;
}
int main(int argc,char**argv) {
    if(argc!=2)return 2;
    if(!brd_ui::dirty_presenter::begin())return 3;
    brd_ui::Model model{};
    model.measurement.value=2500U;
    model.measurement.loaded=true;
    model.battery=84U;
    model.voltage_mv=4100U;
    model.connected=true;
    model.load_adc_valid=true;
    model.rpm_adc_valid=true;
    model.ir1=true;
    model.ir2=true;
    model.load_adc_raw=3205U;
    model.rpm_adc_raw=600U;
    model.history_count=4U;
    model.history[0]=29980U;
    model.history[1]=27360U;
    model.history[2]=25100U;
    model.history[3]=21760U;
    brd_ui::Curve curve{};
    curve.count=32U;
    curve.peak=29980U;
    for(unsigned i=0;i<curve.count;++i)
        curve.rpm[i]=(uint16_t)(i<7U?i*4282U:29980U-(i-7U)*1120U);
    const brd_ui::Page pages[]={brd_ui::Page::Main,brd_ui::Page::Graph,
        brd_ui::Page::History,brd_ui::Page::IrTest,brd_ui::Page::Main};
    for(unsigned style=0;style<3;++style){
        brd_ui::style_scene::draw(*brd_ui::dirty_presenter::canvas(),
            (uint8_t)style,pages[0],0U,model,curve);
        brd_ui::dirty_presenter::invalidate();
        brd_ui::dirty_presenter::flush(brd_ui::lcd());
        for(unsigned page=0;page<4;++page){
            if(!export_frame(argv[1],style,page,0))return 4;
            if(!brd_ui::transition::start(pages[page],pages[page+1],
                (uint8_t)style,model,curve,1000U))return 5;
            for(unsigned step=1;step<=5;++step){
                brd_ui::transition::update(1000U+(step-1U)*80U);
                if(!export_frame(argv[1],style,page,step))return 6;
            }
        }
    }
    return 0;
}
