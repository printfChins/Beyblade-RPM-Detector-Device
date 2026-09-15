/* 檔案位置: BRD_BLE_OLED/brd_power.cpp
   [V1.15 修改] 移除待機 OLED OFF 與 Deep-sleep。
   裝置只要電源與電池條件允許，就維持正常運作；低電保護仍由 brd_battery / 主程式處理。 */
#include "brd_oled.h"
#include "brd_power.h"

void brd_power_begin(void) {
    /* [V1.15 修改] 不建立待機計時；確保 OLED 不因舊待機要求保持關閉。 */
    brd_oled_set_idle_off(false);
}

void brd_power_update(void) {
    /* [V1.15 修改] 待機不再關閉 OLED，也不進入 Deep-sleep。 */
    brd_oled_set_idle_off(false);
}
