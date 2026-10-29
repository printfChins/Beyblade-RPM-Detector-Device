/*
 * File: BRD_BPX_PRO/tests/test_w1_l2_layout.cpp
 * [修改] TORQUE W1/L2 畫素範圍及文字安全間隔回歸測試。
 * [保留] 測試只使用正式 brd_ui_style_scene::draw()，不會寫入 MCU 程式。
 */
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"
#include "brd_ui_legible_text.h"

namespace {
using Image=std::vector<uint16_t>;
Image capture(uint8_t style,bool loaded,uint8_t frame) {
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    brd_ui::Model model{};
    model.measurement.loaded=loaded;
    model.measurement.value=0U;
    model.battery=78U;
    model.voltage_mv=4030U;
    model.connected=true;
    brd_ui::Curve curve{};
    brd_ui::style_scene::draw(canvas,style,brd_ui::Page::Main,frame,model,curve);
    const auto *pixels=static_cast<const uint16_t*>(canvas.getBuffer());
    return Image(pixels,pixels+160*80);
}
}
int main() {
    // [修改] TORQUE 固定狀態區 x=91..159，y=48..58。
    const int x_min[1] = {91};
    const int x_max[1] = {159};
    const int y_top[1] = {48};
    const int compact_wait_width = 54;  // 9 個 6 px 字元（WAIT LOAD）
    const int compact_loaded_width = 36; // 6 個 6 px 字元（LOADED）
    for(uint8_t style=0U;style<1U;++style) {
        const Image waiting0=capture(style,false,0U);
        const Image waiting12=capture(style,false,12U);
        const Image loaded0=capture(style,true,0U);
        const Image loaded8=capture(style,true,8U);
        const int wait_x=(x_min[style]+x_max[style]-compact_wait_width)/2;
        const int loaded_x=(x_min[style]+x_max[style]-compact_loaded_width)/2;
        // [驗證] W1 與 L2 中心重合（奇數寬的區域允許 <1 px 取整差）。
        assert(std::abs((wait_x*2+compact_wait_width)-(loaded_x*2+compact_loaded_width))<=1);
        assert(waiting0!=waiting12);
        assert(loaded0!=loaded8);
        {
            // [刪除確認] TORQUE 左側面板原 LOAD 區域維持背景色，
            // [保留確認] 右側 W1/L2 動畫仍會變化。
            const uint16_t bg=waiting0[53*160+30];
            for(int y=53;y<=60;++y) for(int x=4;x<=26;++x) {
                assert(waiting0[y*160+x]==bg);
                assert(loaded0[y*160+x]==bg);
                assert(loaded8[y*160+x]==bg);
            }
        }
        unsigned breath_delta=0U,focus_delta=0U;
        for(int y=0;y<80;++y) for(int x=0;x<160;++x) {
            const int i=y*160+x;
            if(waiting0[i]!=waiting12[i]) {
                // [驗證] W1 變動只在置中的文字框內，右側沒有多出的呼吸點。
                assert(x>=wait_x && x<wait_x+compact_wait_width);
                assert(y>=y_top[style] && y<=y_top[style]+10);
                ++breath_delta;
            }
            if(loaded0[i]!=loaded8[i]) {
                // [驗證] TORQUE L2 只影響右側狀態區。
                {
                    assert(x>=x_min[style] && x<=159);
                    assert(y>=y_top[style]-1 && y<=y_top[style]+10);
                    ++focus_delta;
                }
            }
            // [驗證] L2 的文字本體不受左右括號收束影響。
            if(x>=loaded_x && x<loaded_x+compact_loaded_width &&
               y>=y_top[style] && y<y_top[style]+10) {
                assert(loaded0[i]==loaded8[i]);
            }
            // [保留] 右上 BLE / 電池不能被狀態特效改寫。
            if(y<=16) {
                assert(waiting0[i]==waiting12[i]);
                assert(loaded0[i]==loaded8[i]);
            }
        }
        assert(breath_delta>0U && focus_delta>0U);
    }
    std::puts("PASS: TORQUE W1/L2 centered, left LOAD removed.");
}
