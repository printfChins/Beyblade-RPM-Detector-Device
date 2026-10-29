/*
 * File: BRD_BPX_PRO/tests/test_ui_indicator_full_scale.cpp
 * [修改] 2026-10-29：只驗證 TORQUE 的 16 段轉速條，畫面峰值不被截斷。
 */
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>
#include "brd_config.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"

namespace {
uint16_t rgb(int r,int g,int b) {
    return static_cast<uint16_t>(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
size_t segments(const GFXcanvas16& cv,uint16_t color) {
    size_t count=0U;
    for(int i=0;i<16;++i) {
        if(cv.pixels[68*160+61+i*6]==color)++count;
    }
    return count;
}
void render(GFXcanvas16& cv,brd_ui::Model& m,uint32_t rpm) {
    brd_ui::Curve curve{};
    m.measurement.value=rpm;
    m.measurement.peak_value=rpm;
    brd_ui::style_scene::draw(cv,0U,brd_ui::Page::Main,4U,m,curve);
}
}
int main() {
    static_assert(BRD_UI_INDICATOR_FULL_SCALE_RPM>0UL,"full scale must be positive");
    GFXcanvas16 cv(160,80);
    cv.setTextWrap(false);
    brd_ui::Model m{};
    m.battery=80U;
    m.connected=true;
    const uint16_t orange=rgb(250,124,77);
    const uint32_t full=BRD_UI_INDICATOR_FULL_SCALE_RPM;
    render(cv,m,0U);
    assert(segments(cv,orange)==0U);
    render(cv,m,full/2U);
    assert(segments(cv,orange)==8U);
    render(cv,m,full);
    assert(segments(cv,orange)==16U);
    render(cv,m,full+full/3U);
    assert(segments(cv,orange)==16U);
    // [防呆] 舊風格索引不能產生不同畫面。
    const auto full_snapshot=std::vector<uint16_t>(cv.pixels,cv.pixels+160U*80U);
    brd_ui::Curve curve{};
    brd_ui::style_scene::draw(cv,2U,brd_ui::Page::Main,4U,m,curve);
    assert(std::equal(full_snapshot.begin(),full_snapshot.end(),cv.pixels));
    std::puts("PASS: TORQUE single theme, 16 RPM segments saturate at full scale.");
}
