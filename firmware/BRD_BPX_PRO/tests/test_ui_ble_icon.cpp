/* BRD_BPX_PRO/tests/test_ui_ble_icon.cpp
 * [修改] TORQUE: BLE glyph reacts to link/disabled states, without
 * touching battery or any pixel outside the intended glyph bounds.
 * [保留] TORQUE W1/L2 裝載狀態與 BLE 顯示不重疊。
 * [保留] BLE icon / battery slot 不得被 LOAD 狀態特效污染。
 */
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"

static std::vector<uint16_t> render(uint8_t style, bool link, bool disabled,
                                    bool loaded, bool hold) {
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Model m{};
    m.measurement.value=2500;
    m.measurement.loaded=loaded;
    m.measurement.hold_active=hold;
    m.mode_auto=false;
    m.connected=link;
    m.ble_disabled=disabled;
    m.battery=84;
    m.voltage_mv=4100;
    brd_ui::Curve c{};
    brd_ui::style_scene::draw(canvas,style,brd_ui::Page::Main,4,m,c);
    const auto *p=static_cast<const uint16_t*>(canvas.getBuffer());
    return {p,p+160U*80U};
}

int main() {
    {
        const uint8_t style=0U;
        const auto online=render(style,true,false,true,false);
        const auto waiting=render(style,false,false,true,false);
        const auto disabled=render(style,false,true,true,false);
        const auto unloaded=render(style,true,false,false,false);
        const auto hold=render(style,true,false,true,true);
        // [修改] 只檢驗 TORQUE 的獨立 LOAD 區域。
        assert(online!=unloaded);
        {
            for(int y=0;y<80;++y) for(int x=0;x<160;++x) {
                if(online[y*160+x]==unloaded[y*160+x]) continue;
                assert(x>=91 && x<=159 && y>=47 && y<=59);
            }
        }
        assert(online==hold);
        uint32_t link_changes=0U, disabled_changes=0U;
        for(int y=0;y<80;++y)for(int x=0;x<160;++x) {
            const int idx=y*160+x;
            if(online[idx]!=waiting[idx]) {
                assert(x>=92 && x<=101 && y>=2 && y<=14);
                ++link_changes;
            }
            if(waiting[idx]!=disabled[idx]) {
                assert(x>=92 && x<=101 && y>=2 && y<=14);
                ++disabled_changes;
            }
        }
        assert(link_changes>=12U);
        assert(disabled_changes>=12U);
        // Battery icon and percentage must remain pixel-identical.
        for(int y=0;y<=16;++y)for(int x=105;x<160;++x)
            assert(online[y*160+x]==waiting[y*160+x] &&
                   online[y*160+x]==disabled[y*160+x] &&
                   online[y*160+x]==unloaded[y*160+x]);
    }
    std::puts("PASS: TORQUE isolated BLE/battery and W1/L2 state zones.");
}
