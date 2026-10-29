/* 檔案位置: BRD_BPX_PRO/brd_io.h
   [PRO 修改] 電源鎖定、IR 電源、按鈕與新 TFT GPIO。 */
#ifndef BRD_IO_H
#define BRD_IO_H
void brd_io_begin(void);
bool brd_io_is_charging(void);
bool brd_io_read_ir1(void);
bool brd_io_read_ir2(void);
/* [PRO 新增] HIGH/LOW 可由 brd_config.h 修改。 */
void brd_io_set_ir_enabled(bool enabled);
bool brd_io_button_pressed(void);
#endif
