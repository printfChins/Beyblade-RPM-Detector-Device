/*
 * File: BRD_BPX_PRO/tests/render_torque_preview.cpp
 * [新增] 以正式 TORQUE GFX scene 輸出完整頁面及 25 FPS 原始動畫影格。
 * [注意] 僅為 PC 主機端檢視工具，不加入 Arduino 韌體。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstring>
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"

namespace {
void save(const char *folder,const char* filename,brd_ui::Page page,
          const brd_ui::Model& model,const brd_ui::Curve& curve,uint8_t frame) {
    GFXcanvas16 cv(160,80);
    cv.setTextWrap(false);
    brd_ui::style_scene::draw(cv,0U,page,frame,model,curve);
    char dest[512];
    std::snprintf(dest,sizeof dest,"%s/%s.rgb565",folder,filename);
    FILE *f=std::fopen(dest,"wb");
    if(!f) return;
    (void)std::fwrite(cv.getBuffer(),sizeof(uint16_t),160U*80U,f);
    std::fclose(f);
}
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    const char *out=argv[1];
    brd_ui::Model m{};
    m.battery=84U;
    m.voltage_mv=4020U;
    m.connected=true;
    m.load_adc_valid=true;
    m.rpm_adc_valid=true;
    m.load_adc_raw=1780U;
    m.rpm_adc_raw=2870U;
    m.ir1=true;
    m.ir2=true;
    m.history_count=4U;
    m.history[0]=29980U;
    m.history[1]=28240U;
    m.history[2]=25100U;
    m.history[3]=22810U;
    brd_ui::Curve curve{};
    curve.count=32U;
    curve.peak=29980U;
    for(unsigned i=0;i<curve.count;++i)
        curve.rpm[i]=static_cast<uint16_t>(i<7U?i*4282U:29980U-(i-7U)*1120U);

    // [新增] 每類畫面個別 PNG 的原始 RGB565 素材（稍後轉檔）。
    m.measurement.loaded=false;
    m.measurement.value=0U;
    save(out,"01_WAIT_LOAD",brd_ui::Page::Main,m,curve,12U);
    m.measurement.loaded=true;
    save(out,"02_LOADED",brd_ui::Page::Main,m,curve,8U);
    m.measurement.loaded=false;
    m.measurement.value=15880U;
    m.measurement.peak_value=15880U;
    save(out,"03_LIVE_SP",brd_ui::Page::Main,m,curve,10U);
    m.measurement.value=29980U;
    m.measurement.peak_value=29980U;
    m.measurement.show_max=true;
    save(out,"04_MAX_SP",brd_ui::Page::Main,m,curve,10U);
    save(out,"05_MAX_RESULT",brd_ui::Page::Max,m,curve,7U);
    save(out,"06_CURVE",brd_ui::Page::Graph,m,curve,14U);
    save(out,"07_HISTORY",brd_ui::Page::History,m,curve,14U);
    save(out,"08_IR_TEST",brd_ui::Page::IrTest,m,curve,0U);
    save(out,"09_BOOT",brd_ui::Page::Boot,m,curve,14U);
    m.ota.percent=65U;
    save(out,"10_OTA",brd_ui::Page::Ota,m,curve,14U);
    save(out,"11_LOW_BATTERY",brd_ui::Page::LowBattery,m,curve,0U);
    save(out,"12_ADC_FAULT",brd_ui::Page::AdcFault,m,curve,1U);

    // [新增] 25 FPS，W1 / L2 / LIVE / MAX 各 24 幀，共 96 幀。
    for(unsigned i=0U;i<96U;++i) {
        m.measurement.show_max=i>=72U;
        m.measurement.loaded=i>=24U&&i<48U;
        m.measurement.value=i<48U?0U:(i<72U?15880U:29980U);
        m.measurement.peak_value=m.measurement.value;
        char filename[64];
        std::snprintf(filename,sizeof filename,"frame_%03u",i);
        save(out,filename,brd_ui::Page::Main,m,curve,
             static_cast<uint8_t>(i%24U));
    }
    return 0;
}
