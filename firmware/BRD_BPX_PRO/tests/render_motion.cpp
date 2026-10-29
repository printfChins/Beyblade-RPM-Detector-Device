/* Host-only renderer/test. Uses the same brd_ui_motion.cpp source shipped to MCU. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Arduino.h"
#include "brd_ui_screen.h"
#include "brd_ui_motion.h"
#include "brd_ui_curve.h"
#include "SPI.h"
uint64_t test_clock_us = 0;
void (*test_interrupts[32])(void) = {};
namespace brd_ui {
static Adafruit_ST7735 mock_lcd(&SPI,-1,2,-1);
Adafruit_ST7735 &lcd() { return mock_lcd; }
}
int main(int argc, char **argv) {
    if(argc!=4)return 2;
    const int theme=std::atoi(argv[1]);
    const int pageid=std::atoi(argv[2]);
    const char *outdir=argv[3];
    const brd_ui::Page pages[]={brd_ui::Page::Main,brd_ui::Page::Max,
        brd_ui::Page::History,brd_ui::Page::Graph,brd_ui::Page::IrTest,
        brd_ui::Page::LowBattery,brd_ui::Page::AdcFault};
    if(theme!=0||pageid<0||pageid>6)return 3; // [修改] TORQUE only.
    brd_ui::Model m{};
    m.battery=76;
    m.voltage_mv=3990;
    m.measurement.loaded=true;
    m.measurement.value=16420;
    m.history_count=4;
    const uint16_t history[]={29980,25700,21520,18770};
    std::memcpy(m.history,history,sizeof(history));
    m.load_adc_valid=m.rpm_adc_valid=true;
    m.load_adc_raw=1750;m.rpm_adc_raw=2850;
    brd_ui::Curve curve{};
    curve.count=50;
    curve.peak=29980;
    curve.version=1;
    for(int i=0;i<50;i++){
        const int speed=i<8?2000+i*3500: (i<25?29980-(i-8)*1000:12980-(i-25)*440);
        curve.rpm[i]=(uint16_t)(speed<0?0:speed);
    }
    if(pageid==1){m.measurement.show_max=true;m.measurement.value=29980;m.measurement.hold_active=true;}
    if(pageid==5)m.low=true;
    if(pageid==6)m.adc_fault=true;
    brd_ui::motion::begin((uint8_t)theme);
    brd_ui::motion::enter(pages[pageid],0);
    const uint32_t target_frames=16;
    for(uint32_t n=0;n<target_frames;n++){
        const uint32_t now=pageid>=5?n*300U:n*60U;
        if(pageid==0){m.measurement.value=(uint16_t)(n<8?5000+n*1700:16420);}
        if(pageid==4){m.ir2=(n>=4&&n<12);m.ir1=(n>=8);m.load_adc_raw=(uint16_t)(n*215U);m.rpm_adc_raw=(uint16_t)(200+n*110U);}
        brd_ui::motion::update(pages[pageid],m,curve,now);
        char path[512];
        std::snprintf(path,sizeof(path),"%s/t%02d_p%d_f%02u.rgb565",outdir,theme,pageid,(unsigned)n);
        FILE *f=std::fopen(path,"wb");if(!f)return 4;
        std::fwrite(brd_ui::lcd().pixels,sizeof(uint16_t),160U*80U,f);
        std::fclose(f);
    }
    if(brd_ui::lcd().full_clear_count!=1U)return 6;
    return 0;
}
