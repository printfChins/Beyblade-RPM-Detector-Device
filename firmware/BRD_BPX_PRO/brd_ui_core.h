/* 檔案: BRD_BPX_PRO/brd_ui_core.h
 * [新增] 頁面路由器與 Dirty 區域調度；不含硬體/量測/藍牙取值邏輯。
 */
#ifndef BRD_UI_CORE_H
#define BRD_UI_CORE_H
#include "brd_ui_types.h"
namespace brd_ui {
bool begin();
void show_boot();
void animate_boot(uint32_t duration_ms); /* [新增] 原開機等待期間顯示逐格動畫 */
bool render(const Model &model);
bool ready();
Page current_page();
}
#endif
