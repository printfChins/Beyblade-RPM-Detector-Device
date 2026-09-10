/* 檔案位置: BRD_OLED_V0.10/brd_io.h
   [V0.10 修改] GPIO 初始化與充電 DET 讀取。 */
#ifndef BRD_IO_H
#define BRD_IO_H

void brd_io_begin(void);
/* [V0.10 恢復] GPIO10 LOW 表示充電中，供 OLED 充電圖示使用。 */
bool brd_io_is_charging(void);

#endif
