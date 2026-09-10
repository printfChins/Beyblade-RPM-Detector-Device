/* 檔案位置: BRD_OLED_V0.10/brd_oled.h
   [V0.10 修改] 一般畫面由主 loop 分段寫入；低電停用量測後完整送出警示。 */
#ifndef BRD_OLED_H
#define BRD_OLED_H

void brd_oled_begin(void);
void brd_oled_update(void);

#endif
