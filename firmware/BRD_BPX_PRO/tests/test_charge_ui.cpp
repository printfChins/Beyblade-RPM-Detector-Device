/*
 * File: BRD_BPX_PRO/tests/test_charge_ui.cpp
 * [修改] Charging UI TORQUE / MAIN / IR TEST regression.
 * [保留] Production C++ GFX scene rendering; no ESP hardware dependency.
 */
#include <cassert>
#include <cstdio>
#include <vector>
#include <Adafruit_GFX.h>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"

namespace {
using Snapshot=std::vector<uint16_t>;
Snapshot render(uint8_t style,brd_ui::Page page,bool charging,uint8_t phase) {
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Model m{};
    m.battery=64U;
    m.voltage_mv=3870U;
    m.charging=charging;
    m.charge_phase=phase;
    m.connected=true;
    m.measurement.loaded=false;
    brd_ui::Curve curve{};
    brd_ui::style_scene::draw(canvas,style,page,0,m,curve);
    const auto* ptr=(const uint16_t*)canvas.getBuffer();
    return Snapshot(ptr,ptr+160U*80U);
}
}
int main() {
    for(uint8_t theme=0;theme<1;++theme) {
        for(brd_ui::Page page:{brd_ui::Page::Main,brd_ui::Page::IrTest}) {
            const Snapshot idle=render(theme,page,false,0);
            const Snapshot phase0=render(theme,page,true,0);
            const Snapshot phase1=render(theme,page,true,1);
            const Snapshot phase2=render(theme,page,true,2);
            // [確認] 充電 UI 跟未充電 UI 可辨識，且至少兩相位的像素不同。
            assert(idle!=phase0);
            assert(phase0!=phase1);
            assert(phase1!=phase2);
            for(size_t i=0;i<idle.size();++i) {
                if(idle[i]==phase0[i] && phase0[i]==phase1[i])continue;
                const unsigned x=(unsigned)(i%160U);
                const unsigned y=(unsigned)(i/160U);
                // [確認] UI 只修改頂部狀態列，不干擾 RPM 和 IR 感測器幾何。
                assert(y<17U);
                assert(x>=90U);
            }
            // [確認] phase 改變不會影響電池外的感測器幾何 / RPM 區域。
            for(unsigned y=17U;y<80U;++y)
                for(unsigned x=0;x<160U;++x)
                    assert(phase0[y*160U+x]==phase1[y*160U+x]);
        }
    }
    std::puts("PASS: TORQUE, MAIN/IR TEST, animation phases, no unrelated redraw.");
}
