/*
 * File: BRD_BPX_PRO/brd_ui_page_ota.cpp
 * [修改] OTA 實機畫面改與預覽共用 style_scene::draw()，TORQUE 百分比/驗證/完成一致。
 * [修改] 狀態變化時透過 Dirty Tile 局部上傳，不再手動重畫整張 LCD。
 * [保留] brd_ui::page_ota::enter()/update() 公開介面。
 */
#include <Arduino.h>
#include "brd_config.h"
#include "brd_ui_pages.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_curve.h"
#include "brd_ui_dirty_presenter.h"
#include "brd_ui_screen.h"
namespace brd_ui { namespace page_ota {
namespace {
struct Cache {
    bool valid=false;
    uint8_t percent=0;
    uint8_t phase=0;
    bool verifying=false;
    uint8_t battery=0;
    bool charging=false;
};
Cache cache{};
} // namespace
void enter() {
    cache={};
    // [新增] 由 Dirty Tile 負責整頁第一次傳送的無效化。
    dirty_presenter::invalidate();
}
bool update(const Model &model) {
    if (!dirty_presenter::canvas()) return false;
    const uint8_t pct=model.ota.percent>100U?100U:model.ota.percent;
    const uint8_t phase=static_cast<uint8_t>(model.ota.phase);
    if (cache.valid && cache.percent==pct && cache.phase==phase &&
        cache.verifying==model.ota.verifying && cache.battery==model.battery &&
        cache.charging==model.charging) return false;
    // [修改] 共用正式的 RGB565 場景繪圖，GFXcanvas16 分配於 Presenter。
    // OTA 本身靠真實進度推動，無須播放額外非必要逐幀動畫。
    Curve empty_curve{};
    style_scene::draw(*dirty_presenter::canvas(),BRD_UI_MOTION_THEME,
                      Page::Ota,static_cast<uint8_t>(pct/12U),model,empty_curve);
    const uint32_t sent_bytes=dirty_presenter::flush(lcd());
    cache.valid=true;
    cache.percent=pct;
    cache.phase=phase;
    cache.verifying=model.ota.verifying;
    cache.battery=model.battery;
    cache.charging=model.charging;
    return sent_bytes!=0U;
}
} }
