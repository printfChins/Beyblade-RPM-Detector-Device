/*
 * File: BRD_BPX_PRO/tests/render_exact_style.cpp
 * [新增] 使用正式 TORQUE 單風格程式 + Dirty Presenter 導出 GIF 測試原始幀。
 * [說明] 此檔只編譯在桌面端，不會燒錄到 MCU。
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Arduino.h"
#include "brd_ui_screen.h"
#include "brd_ui_motion.h"
#include "brd_ui_dirty_presenter.h"
#include "brd_ui_curve.h"
#include "SPI.h"
uint64_t test_clock_us=0;
void (*test_interrupts[32])(void)={};
namespace brd_ui {
static Adafruit_ST7735 mock_lcd(&SPI,-1,2,-1);
Adafruit_ST7735 &lcd() {return mock_lcd;}
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const int style=std::atoi(argv[1]);
    const int page_id=std::atoi(argv[2]);
    const char* outdir=argv[3];
    if(style!=0||page_id<0||page_id>=7)return 3; // [修改] TORQUE only.
    constexpr brd_ui::Page pages[]={brd_ui::Page::Main,brd_ui::Page::Max,
        brd_ui::Page::History,brd_ui::Page::Graph,brd_ui::Page::IrTest,
        brd_ui::Page::LowBattery,brd_ui::Page::AdcFault};
    brd_ui::Model m{};m.battery=84;m.voltage_mv=4010;
    m.connected=true;m.measurement.loaded=true;m.measurement.value=2500;
    m.history_count=4;
    uint16_t history[4]={29980,27360,25100,21760};
    std::memcpy(m.history,history,sizeof(history));
    m.load_adc_raw=1400;m.rpm_adc_raw=2500;
    m.load_adc_valid=m.rpm_adc_valid=true;
    brd_ui::Curve curve{};curve.count=36;curve.peak=30400;curve.frozen=true;curve.version=1;
    for(unsigned i=0;i<curve.count;++i) {
       uint32_t speed=i<10?2200U+i*3100U:30400U-(i-10U)*1150U;
       curve.rpm[i]=(uint16_t)((int32_t)speed<0?0:speed);
    }
    if(page_id==1){m.measurement.show_max=true;m.measurement.hold_active=true;m.measurement.value=30400;}
    if(page_id==5)m.low=true;
    if(page_id==6)m.adc_fault=true;
    brd_ui::motion::begin((uint8_t)style);
    if(!brd_ui::motion::ready())return 6;
    brd_ui::motion::enter(pages[page_id],0);
    for(uint32_t n=0;n<20U;++n) {
       uint32_t now=n*(((page_id==5)||(page_id==6))?500U:120U);
       if(page_id==0) m.measurement.value=(uint16_t)(2500U+n*2000U);
       if(page_id==4) {
           m.ir2=(n>=3U && n<10U);
           m.ir1=(n>=6U);
           m.load_adc_raw=(uint16_t)(1400U+n*95U);
           m.rpm_adc_raw=(uint16_t)(2500U-n*100U);
       }
       brd_ui::motion::update(pages[page_id],m,curve,now);
       const uint16_t* pixels=brd_ui::lcd().pixels;
       char dest[640];
       std::snprintf(dest,sizeof(dest),"%s/t%02d_p%d_f%02u.rgb565",outdir,style,page_id,(unsigned)n);
       FILE *f=std::fopen(dest,"wb");if(!f)return 4;
       std::fwrite(pixels,sizeof(uint16_t),160U*80U,f);
       std::fclose(f);
    }
    std::printf("%d,%d,%lu,%lu\n",style,page_id,
         (unsigned long)brd_ui::lcd().transferred_bytes,
         (unsigned long)brd_ui::lcd().transfer_count);
    if(brd_ui::lcd().transferred_bytes>20U*25600U) return 7;
    return 0;
}
