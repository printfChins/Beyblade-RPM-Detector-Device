/*
 * File: BRD_BPX_PRO/tests/test_nebula_three_phase.cpp
 * [新增] NEBULA 未裝載/裝載/發射/MAX 四階段及全部三主題峰值顯示回歸。
 * 本檔僅用於主機端測試；不會編入 ESP32-C3 Arduino 主程式。
 */
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"

static uint16_t rgb(int r,int g,int b) {
    return static_cast<uint16_t>(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}

static size_t count_ring(const GFXcanvas16& canvas,uint16_t color) {
    const uint16_t *pixels=canvas.pixels;
    size_t count=0;
    for(int y=5;y<75;++y) {
        for(int x=7;x<75;++x) {
            const int dx=x-41,dy=y-40;
            const int radius2=dx*dx+dy*dy;
            if(radius2>=17*17 && radius2<=34*34 && pixels[y*160+x]==color) {
                ++count;
            }
        }
    }
    return count;
}

static std::vector<uint16_t> capture(GFXcanvas16& canvas,brd_ui::Model& m,
                                     uint8_t theme,uint8_t tick) {
    brd_ui::Curve curve{};
    brd_ui::style_scene::draw(canvas,theme,brd_ui::Page::Main,tick,m,curve);
    const uint16_t *p=static_cast<const uint16_t*>(canvas.getBuffer());
    return std::vector<uint16_t>(p,p+160U*80U);
}

int main() {
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Model m{};
    m.battery=84U;
    m.connected=true;
    const uint16_t blue=rgb(75,227,228);
    const uint16_t yellow=rgb(255,211,61);

    // [修改] 未裝載也有內外圈底環；只是藍色指示弧與黃色旋轉特效尚未點亮。
    m.measurement.loaded=false;
    m.measurement.value=0;
    m.measurement.peak_value=0;
    const auto waiting=capture(canvas,m,0U,1U);
    assert(count_ring(canvas,blue)==0U);
    assert(count_ring(canvas,yellow)==0U);
    const uint16_t background=rgb(4,10,23);
    assert(waiting[72U*160U+41U]!=background); // outer dark ring at r=32
    assert(waiting[67U*160U+41U]!=background); // intermediate dark ring at r=27
    assert(waiting[61U*160U+41U]!=background); // inner yellow-dim ring at r=21

    // [檢查] 裝載完成即旋轉黃色圈，且尚未發射時藍圈保持熄滅。
    m.measurement.loaded=true;
    const auto ready_first=capture(canvas,m,0U,2U);
    assert(count_ring(canvas,blue)==0U);
    assert(count_ring(canvas,yellow)>0U);
    assert(ready_first[72U*160U+41U]==waiting[72U*160U+41U]); // outer outline stable
    const auto ready_next=capture(canvas,m,0U,3U);
    assert(ready_first!=ready_next);
    assert(count_ring(canvas,blue)==0U);

    // [檢查] 裝載仍成立時，即使有預轉速，藍圈尚未啟動。
    m.measurement.value=18000U;
    m.measurement.peak_value=18000U;
    capture(canvas,m,0U,4U);
    assert(count_ring(canvas,blue)==0U);

    // [檢查] 發射（LOAD 轉為未裝載）後藍圈才亮，黃圈繼續旋轉。
    m.measurement.loaded=false;
    m.measurement.value=18000U;
    const auto launched=capture(canvas,m,0U,5U);
    assert(count_ring(canvas,blue)>0U);
    assert(count_ring(canvas,yellow)>0U);

    // [檢查] 原始 RPM 降低，但峰值不變，畫面必須完全相同。
    m.measurement.value=5000U;
    m.measurement.peak_value=18000U;
    assert(capture(canvas,m,0U,5U)==launched);

    // [檢查] 真正的 peak 增長時，藍色量表延伸。
    m.measurement.value=21900U;
    m.measurement.peak_value=21900U;
    const auto faster=capture(canvas,m,0U,5U);
    assert(faster!=launched);

    // [檢查] MAX 結束後保存最後峰值，黃色旋轉停止。
    m.measurement.show_max=true;
    m.measurement.value=21900U;
    capture(canvas,m,0U,7U);
    assert(count_ring(canvas,blue)>0U);
    assert(count_ring(canvas,yellow)==0U);

    // [修改] 下一發完成重置後，內外暗色底環仍留在主畫面。
    m.measurement.show_max=false;
    m.measurement.peak_value=0U;
    m.measurement.value=0U;
    const auto idle_after=capture(canvas,m,0U,0U);
    assert(count_ring(canvas,blue)==0U);
    assert(count_ring(canvas,yellow)==0U);
    assert(idle_after[72U*160U+41U]!=background);
    assert(idle_after[61U*160U+41U]!=background);

    // [檢查] 02 DIAL 與 06 TORQUE 的 LIVE 也不能因原始轉速下降而下降。
    m.measurement.loaded=true;
    m.measurement.peak_value=22000U;
    m.measurement.value=22000U;
    for(uint8_t theme=1U;theme<3U;++theme) {
        const auto before=capture(canvas,m,theme,5U);
        m.measurement.value=9000U;
        assert(before==capture(canvas,m,theme,5U));
        m.measurement.value=22000U;
    }

    std::puts("PASS: NEBULA persistent dual-ring outlines, yellow animation, LIVE RPM arc, MAX and all-three-theme peak hold");
    return 0;
}
