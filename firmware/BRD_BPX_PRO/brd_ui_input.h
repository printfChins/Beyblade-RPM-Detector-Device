/*
 * 檔案: BRD_BPX_PRO/brd_ui_input.h
 * [修改] GPIO5 單鍵四頁短按導覽。
 * [說明] 放開且通過去抖才切頁，回傳 true 表示實際切換。
 */
#ifndef BRD_UI_INPUT_H
#define BRD_UI_INPUT_H

#include <stdint.h>

namespace brd_ui {
void input_begin();
bool input_update(bool allow_switch);
bool input_graph_selected(); // [保留] 相容原有 UI 呼叫介面。
uint8_t input_current_page(); // 0=Main/Max, 1=Graph, 2=History, 3=IrTest。
} // namespace brd_ui

#endif
