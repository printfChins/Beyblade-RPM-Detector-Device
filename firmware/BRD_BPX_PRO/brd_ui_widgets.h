/* 檔案位置: BRD_BPX_PRO/brd_ui_widgets.h
 * [修改] 頂部通知列：BLE 圖示 / 電池內流光進行 Dirty 更新；移除額外充電圖示。
 * [修改] MODE 統一移到一般頁面底部；IR TEST 亦沿用此共用元件。
 */
#ifndef BRD_UI_WIDGETS_H
#define BRD_UI_WIDGETS_H
#include "brd_ui_types.h"
namespace brd_ui {
struct HeaderCache {
    bool valid = false;
    uint8_t percent = 0;
    bool charging = false;
    uint8_t charge_phase = 0U; // [新增] 局部刷新時追蹤充電動畫相位。
    bool connected = false;
    bool disabled = false;
};
struct FooterCache {
    bool valid = false;
    bool mode_auto = false;
};
void header_static(const char *title);
/* [保留] 原本函式名稱，以免影響 OTA / Graph 頁面。 */
bool header_battery(const Model &model, HeaderCache &cache);
void footer_static();
bool footer_update(const Model &model, FooterCache &cache, bool home_page = false);
}
#endif
