/* 檔案: BRD_BPX_PRO/brd_ui_pages.h
 * [新增] 頁面模組介面：enter 只在切頁時建立固定背景，update 僅處理變更欄位。
 * 新頁面：新建 brd_ui_page_xxx.cpp，在本檔宣告 enter/update 並在 core 註冊。
 */
#ifndef BRD_UI_PAGES_H
#define BRD_UI_PAGES_H
#include "brd_ui_types.h"
namespace brd_ui {
namespace page_main { void enter(); bool update(const Model &); }
namespace page_graph { void enter(); bool update(const Model &); }
namespace page_ir { void enter(); bool update(const Model &); }
namespace page_ota { void enter(); bool update(const Model &); }
namespace page_alert { void enter(Page); }
}
#endif
