/*
 * File: BRD_BPX_PRO/brd_ui_motion.cpp
 * [修改] 只保留 TORQUE；主畫面/曲線/歷史動畫依 CFG 排程（40 ms、目標 25 FPS）。
 * [保留] 非阻塞更新、峰值保持、Dirty Tile 局部傳輸及 BLE/OTA 原功能。
 * [刪除] 舊版20種主題只有少數共用版型的簡化繪圖。
 * [保留] Adafruit ST7735/GFX 底層，無自製 ST7735 SPI 及命令表。
 * [注意] 所有動畫排程都在 main loop 中，不從 GPIO ISR 呼叫。
 */
#include <Arduino.h>
#include <cstring>
#include <algorithm>
#include "brd_config.h"
#include "brd_ui_motion.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_dirty_presenter.h"
#include "brd_ui_screen.h"
namespace brd_ui { namespace motion {
namespace {
uint8_t selected_theme=0U;
Page active_page=Page::Boot;
bool entered=false;
bool prepared=false;
uint32_t entered_ms=0U;
uint32_t last_draw_ms=0U;
uint8_t current_frame=0U;
uint32_t last_signature=0U;
uint32_t count_rebuilds=0U;
// [新增] 主畫面即時 SP / MAX SP 轉態時重新開始短暫動態。
bool last_main_show_max=false;
// [新增] WAIT LOAD / LOADED / LIVE / MAX 狀態切換時重新開始 W1、L2 幀序。
uint8_t last_main_load_state=0xffU;
static uint8_t main_load_state(const Model& m){
    if(m.measurement.show_max) return 3U;
    if(m.measurement.loaded) return 1U;
    if(std::max(m.measurement.value,m.measurement.peak_value)>0U) return 2U;
    return 0U;
}
uint32_t hash_mix(uint32_t h,uint32_t value) {
    h^=value;
    return h*16777619UL;
}
uint32_t signature(const Model& m,const Curve& curve,Page page) {
    uint32_t h=2166136261UL;
    h=hash_mix(h,(uint32_t)page);
    // [修改] 只追蹤畫面峰值，避免原始 RPM 下降仍觸發無意義重繪。
    h=hash_mix(h,std::max(m.measurement.value,m.measurement.peak_value));
    h=hash_mix(h,m.measurement.generation);
    h=hash_mix(h,m.measurement.loaded);
    h=hash_mix(h,m.measurement.hold_active);
    h=hash_mix(h,m.measurement.show_max);
    h=hash_mix(h,m.mode_auto);
    h=hash_mix(h,m.battery);
    h=hash_mix(h,m.charging);
    // [新增] MAIN / IR TEST 靜止畫面在充電相位改變時也能局部刷新。
    // [保留] 其他頁不因動畫相位而額外更新。
    if(m.charging && (page==Page::Main || page==Page::IrTest)) {
        h=hash_mix(h,m.charge_phase);
    }
    h=hash_mix(h,m.connected);
    h=hash_mix(h,m.ble_disabled); // [新增] BLE enabled/disabled icon redraw trigger.
    h=hash_mix(h,m.load_adc_raw);
    h=hash_mix(h,m.rpm_adc_raw);
    h=hash_mix(h,m.load_adc_valid);
    h=hash_mix(h,m.rpm_adc_valid);
    h=hash_mix(h,m.ir1);
    h=hash_mix(h,m.ir2);
    h=hash_mix(h,curve.version);
    h=hash_mix(h,curve.peak);
    h=hash_mix(h,curve.count);
    h=hash_mix(h,m.history_count);
    h=hash_mix(h,m.history_peak);
    for(uint8_t i=0;i<8U;++i) h=hash_mix(h,m.history[i]);
    return h;
}
uint16_t interval(Page page) {
    // [修改] 各頁 FPS 從 brd_config.h 統一調整，不再固定寫死於動畫核心。
    switch(page) {
        case Page::Main: return BRD_UI_MAIN_FRAME_INTERVAL_MS;
        case Page::Graph: return BRD_UI_GRAPH_FRAME_INTERVAL_MS;
        case Page::History: return BRD_UI_HISTORY_FRAME_INTERVAL_MS;
        case Page::IrTest: return BRD_UI_IR_FRAME_INTERVAL_MS;
        case Page::LowBattery:
        case Page::AdcFault: return BRD_UI_WARNING_FRAME_INTERVAL_MS;
        default: return BRD_UI_MAIN_FRAME_INTERVAL_MS;
    }
}
bool needs_animation(Page page,uint8_t frame,const Model& m) {
    switch(page) {
        case Page::Main: {
            const uint8_t state=main_load_state(m);
            // [保留] TORQUE WAIT LOAD 持續呼吸。
            if(state==0U) return true;
            // [修改] TORQUE 原索引 2：LOADED 只更新 9 幀，避免無限旋轉。
            if(state==1U) return frame<9U;
            return state==3U ? frame<9U :
                std::max(m.measurement.value,m.measurement.peak_value)>=1000U;
        }
        case Page::Boot: return frame<17U; /* [新增] 8 段逐段滿格，40 ms / 幀 */
        case Page::Max: return frame<11U;
        case Page::Graph: return frame<21U; /* [修改] 4+3*20 >= 64，完整畫完所有曲線點。 */
        case Page::History: return frame<10U;
        case Page::IrTest: return false; // 靜態主題色填滿
        case Page::LowBattery: return true; // [修改] 最後一格紅色電量持續閃爍，直到退出低電頁
        case Page::AdcFault: return frame<8U; // [保留] ADC異常低頻警示四秒後靜止
        default: return false;
    }
}
}
void begin(uint8_t theme_index) {
    selected_theme=(uint8_t)(theme_index%THEME_COUNT);
    prepared=dirty_presenter::begin();
    entered=false;
}
bool ready(){return prepared;}
void set_theme(uint8_t theme_index) {
    selected_theme=(uint8_t)(theme_index%THEME_COUNT);
    entered=false;
    dirty_presenter::invalidate();
}
uint8_t theme() {return selected_theme;}
bool handles(Page p) {
    return p==Page::Boot||p==Page::Main||p==Page::Max||p==Page::History||p==Page::Graph||
        p==Page::IrTest||p==Page::LowBattery||p==Page::AdcFault;
}
void enter(Page p,uint32_t now) {
    active_page=p;
    entered=true;
    entered_ms=now;
    last_draw_ms=now-1000U;
    current_frame=0U;
    last_signature=0U;
    last_main_show_max=false;
    last_main_load_state=0xffU;
    dirty_presenter::invalidate(); // [新增] 只有頁面切換才整頁送出
}
// [ADD] Transition finished: do not invalidate or redraw its final slide frame.
void adopt_rendered(Page page,uint32_t now_ms) {
    active_page = page;
    entered = true;
    entered_ms = now_ms;
    last_draw_ms = now_ms;
    current_frame = 1U;
    last_signature = 0U; // [ADD] Force a data reconciliation after the transition.
    last_main_load_state=0xffU;
}
bool update(Page p,const Model& m,const Curve& curve,uint32_t now) {
    if(!prepared||!handles(p))return false;
    if(!entered||p!=active_page)enter(p,now);
    // [新增] MAIN 上 show_max 的上升/下降緣重置場景幀序，觸發立即局部更新。
    if(p==Page::Main && current_frame!=0U && m.measurement.show_max!=last_main_show_max) {
        current_frame=0U;
        last_draw_ms=now-interval(p);
    }
    if(p==Page::Main){
        const uint8_t state=main_load_state(m);
        if(last_main_load_state!=0xffU && state!=last_main_load_state){
            // [新增] 裝載瞬間從 W1 進入 L2，括號從外側開始收束。
            current_frame=0U;
            last_draw_ms=now-interval(p);
        }
        last_main_load_state=state;
        last_main_show_max=m.measurement.show_max;
    }
    const uint32_t sig=signature(m,curve,p);
    const bool changed=sig!=last_signature;
    const bool anim=needs_animation(p,current_frame,m);
    if(!changed && !anim && current_frame!=0U) return false;
    if(current_frame!=0U && (uint32_t)(now-last_draw_ms)<interval(p))return false;
    // [新增] 只有資料變更或動畫幀需要刷新才進入 GFX 場景繪製。
    const uint8_t frame=current_frame;
    Adafruit_GFX* cv=dirty_presenter::canvas();
    if(!cv)return false;
    style_scene::draw(*cv,selected_theme,p,frame,m,curve);
    const uint32_t bytes=dirty_presenter::flush(lcd());
    last_signature=sig;
    last_draw_ms=now;
    if(p==Page::LowBattery) {
        // [新增] 永續亮滅幀序：0(亮) -> 1(滅) -> 2(亮) -> 1(滅)。
        // [效能] 500 ms 切換一次，不會累積 uint8_t 幀計數造成停止閃爍。
        current_frame=(frame==1U)?2U:1U;
    } else if(p==Page::Main && anim && !m.measurement.show_max) {
        // [保留] 轉速持續運轉時，幀序循環，不會在第 254 幀永久停住。
        // [W1] 96 為 48 幀呼吸週期的整數倍，循環點不產生亮度跳變。
        current_frame=(current_frame>=96U)?1U:(uint8_t)(current_frame+1U);
    } else if(anim && current_frame<254U) {
        ++current_frame;
    } else if(current_frame==0U) {
        current_frame=1U;
    }
    ++count_rebuilds;
    return bytes>0U;
}
} }
