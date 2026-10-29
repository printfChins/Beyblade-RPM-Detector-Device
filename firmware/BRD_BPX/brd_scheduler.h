/* [新增] 檔案位置：BRD_BPX/brd_scheduler.h。背景 Notify 共用額度。 */
#ifndef BRD_SCHEDULER_H
#define BRD_SCHEDULER_H
void brd_scheduler_begin_cycle(void);
bool brd_scheduler_take_notify_slot(void);
#endif
