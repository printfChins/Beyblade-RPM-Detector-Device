/* [新增] 每輪主迴圈最多嘗試一次 Notify，GPIO 有待處理資料時先讓量測執行。 */
#include "brd_measurement.h"
#include "brd_scheduler.h"
static bool g_notify_used = false;
void brd_scheduler_begin_cycle(void) { g_notify_used = false; }
bool brd_scheduler_take_notify_slot(void) {
    if (g_notify_used || !brd_measurement_background_ready()) { return false; }
    g_notify_used = true;
    return true;
}
