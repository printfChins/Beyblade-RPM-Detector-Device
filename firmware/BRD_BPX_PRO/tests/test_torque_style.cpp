/*
 * File: BRD_BPX_PRO/tests/test_torque_style.cpp
 * [修改] 2026-10-29：單一 TORQUE 主題 9 類畫面與警示區域回歸。
 */
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"
int main() {
    GFXcanvas16 cv(160,80);
    cv.setTextWrap(false);
    brd_ui::Model m{};
    m.battery=84U;m.voltage_mv=4010U;m.measurement.loaded=true;
    m.measurement.value=2500U;m.load_adc_valid=m.rpm_adc_valid=true;
    m.load_adc_raw=2000U;m.rpm_adc_raw=1600U;
    m.history_count=4U;m.history[0]=29980U;m.history[1]=25100U;
    brd_ui::Curve c{};c.count=2U;c.peak=2500U;c.rpm[0]=2000U;c.rpm[1]=2500U;
    const brd_ui::Page pages[]={brd_ui::Page::Main,brd_ui::Page::Max,
        brd_ui::Page::History,brd_ui::Page::Graph,brd_ui::Page::IrTest,
        brd_ui::Page::Ota,brd_ui::Page::Boot,brd_ui::Page::LowBattery,
        brd_ui::Page::AdcFault};
    for(brd_ui::Page page:pages) {
        brd_ui::style_scene::draw(cv,0U,page,5U,m,c);
        const uint16_t* p=static_cast<const uint16_t*>(cv.getBuffer());
        assert(p[160U*80U-1U]!=0U);
        const auto expected=std::vector<uint16_t>(p,p+160U*80U);
        brd_ui::style_scene::draw(cv,2U,page,5U,m,c);
        assert(std::memcmp(cv.getBuffer(),expected.data(),160U*80U*sizeof(uint16_t))==0);
    }
    // [保留] TORQUE 警示圖示原始 x=18,y=31,w=43,h=27。
    brd_ui::style_scene::draw(cv,0U,brd_ui::Page::LowBattery,0U,m,c);
    const auto snapshot=std::vector<uint16_t>(cv.pixels,cv.pixels+160U*80U);
    const uint16_t bg=cv.pixels[159U];
    assert(cv.pixels[31U*160U+18U]!=bg);
    assert(cv.pixels[37U*160U+23U]!=bg);
    brd_ui::style_scene::draw(cv,0U,brd_ui::Page::LowBattery,1U,m,c);
    unsigned changes=0U;
    for(unsigned y=0;y<80U;++y)for(unsigned x=0;x<160U;++x) {
        const unsigned i=y*160U+x;
        if(cv.pixels[i]!=snapshot[i]) {
            assert(x>=23U&&x<28U&&y>=36U&&y<53U);
            ++changes;
        }
    }
    assert(changes==85U);
    brd_ui::style_scene::draw(cv,0U,brd_ui::Page::LowBattery,2U,m,c);
    assert(std::memcmp(cv.getBuffer(),snapshot.data(),160U*80U*sizeof(uint16_t))==0);
    // [保留] IR LOAD / RPM 兩個觸發狀態互不影響。
    m.ir1=false;m.ir2=false;
    brd_ui::style_scene::draw(cv,0U,brd_ui::Page::IrTest,5U,m,c);
    const uint16_t none=cv.pixels[51U*160U+56U];
    m.ir2=true;
    brd_ui::style_scene::draw(cv,0U,brd_ui::Page::IrTest,5U,m,c);
    const uint16_t load_on=cv.pixels[51U*160U+56U];
    assert(none!=load_on);
    m.ir2=false;m.ir1=true;
    brd_ui::style_scene::draw(cv,0U,brd_ui::Page::IrTest,5U,m,c);
    assert(cv.pixels[51U*160U+56U]==none);
    std::puts("PASS: TORQUE single-style 9 pages, warning blink, independent IR.");
}
