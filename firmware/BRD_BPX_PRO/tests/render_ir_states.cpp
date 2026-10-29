/* 
 * BRD_BPX_PRO/tests/render_ir_states.cpp
 * [新增] 主機端 IR TEST 畫面導出；同一份正式 style_scene 產生 4 個觸發組合。
 * [注意] 僅供主機端預覽，Arduino 編譯不納入此檔案。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstdlib>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
int main(int argc,char **argv) {
    if (argc != 2) return 1;
    brd_ui::Curve curve{};
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    for (int theme=0;theme<3;++theme) {
        for (int state=0;state<4;++state) {
            brd_ui::Model model{};
            model.battery=84U;
            model.voltage_mv=4100U;
            model.load_adc_valid=model.rpm_adc_valid=true;
            model.ir2=(state & 1)!=0; // LOAD
            model.ir1=(state & 2)!=0; // RPM
            model.load_adc_raw=model.ir2?3205U:890U;
            model.rpm_adc_raw=model.ir1?3500U:600U;
            brd_ui::style_scene::draw(canvas,(uint8_t)theme,brd_ui::Page::IrTest,0U,model,curve);
            char path[500];
            std::snprintf(path,sizeof(path),"%s/theme%02d_ir_state%d.rgb565",argv[1],theme,state);
            FILE *fp=std::fopen(path,"wb"); if (!fp) return 2;
            if (std::fwrite(canvas.getBuffer(),sizeof(uint16_t),160U*80U,fp)!=160U*80U) return 3;
            std::fclose(fp);
        }
    }
    return 0;
}
