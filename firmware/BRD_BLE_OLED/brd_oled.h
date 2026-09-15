/* 檔案位置: BRD_BLE_OLED/brd_oled.h
   [V0.10 修改] 一般畫面由主 loop 分段寫入；低電停用量測後完整送出警示。 */
#ifndef BRD_OLED_H
#define BRD_OLED_H

/* [V1.11 修改] show_boot_screen=false 時只初始化 OLED，不播放版本畫面。 */
void brd_oled_begin(bool show_boot_screen);
void brd_oled_update(void);
/* [V1.15 修改] 待機關屏已停用；介面保留供 power 模組確保 OLED 啟用。 */
void brd_oled_set_idle_off(bool off);
bool brd_oled_idle_is_off(void);

#endif
