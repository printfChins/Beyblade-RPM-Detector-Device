/* File: BRD_BPX_PRO/tests/render_3style_animation.cpp
 * [新增] 三種保留風格的純主機端 160x80 RGB565 動態預覽。
 * [保留] 每一幀由正式 brd_ui_style_scene::draw() 產生，非自製圖片。
 */
#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "brd_ui_style_scene.h"
#include "brd_ui_types.h"
#include "brd_ui_curve.h"

static bool save(const char *root, int style, int page, int frame,
                 const uint16_t *data) {
    char path[512];
    std::snprintf(path,sizeof(path),"%s/style%02d_page%d_frame%02d.rgb565",
                  root,style,page,frame);
    FILE *fp=std::fopen(path,"wb");
    if(!fp)return false;
    const bool ok=(std::fwrite(data,2U,160U*80U,fp)==160U*80U);
    std::fclose(fp);
    return ok;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    const brd_ui::Page pages[]={brd_ui::Page::Main,brd_ui::Page::Graph,
        brd_ui::Page::History,brd_ui::Page::IrTest,
        brd_ui::Page::LowBattery,brd_ui::Page::AdcFault};
    GFXcanvas16 canvas(160,80);
    canvas.setTextWrap(false);
    for(int style=0;style<3;++style){
        for(int page=0;page<6;++page){
            brd_ui::Model m{};
            m.battery=84U;
            m.voltage_mv=4100U;
            m.connected=true;
            m.measurement.loaded=true;
            m.measurement.value=2500U;
            m.load_adc_valid=m.rpm_adc_valid=true;
            m.load_adc_raw=1800;
            m.rpm_adc_raw=2750;
            m.history_count=4U;
            const uint16_t hist[]={29980,27360,25100,21760};
            std::memcpy(m.history,hist,sizeof hist);
            brd_ui::Curve curve{};
            curve.count=32U;
            curve.peak=29980U;
            for(unsigned i=0;i<curve.count;++i){
                curve.rpm[i]=(uint16_t)(i<7U?i*4282U:29980U-(i-7U)*1120U);
            }
            for(int frame=0;frame<12;++frame){
                if(page==0)m.measurement.value=(uint16_t)(2500U+frame*1300U);
                if(page==3){
                    m.ir2=(frame>=3&&frame<9);
                    m.ir1=(frame>=6);
                    m.load_adc_raw=(uint16_t)(900+frame*205);
                    m.rpm_adc_raw=(uint16_t)(300+frame*220);
                }
                brd_ui::style_scene::draw(canvas,(uint8_t)style,pages[page],
                    (uint8_t)frame,m,curve);
                if(!save(argv[1],style,page,frame,
                    static_cast<const uint16_t*>(canvas.getBuffer())))return 3;
            }
        }
    }
    return 0;
}
