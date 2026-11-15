/*
    檔案位置: BRD_BPX/brd_battery_capacity.h
    [V1.4.3 新增] 共用電池容量檔位，設定模型及充電計時共用，避免容量定義不一致。
    [保留] 充電電流固定由 brd_config.h 設定，放電電壓 SOC 查表不更動。
*/
#ifndef BRD_BATTERY_CAPACITY_H
#define BRD_BATTERY_CAPACITY_H

#include <stdint.h>

#define BATTERY_CAPACITY_200_MAH          200U
/* [V1.4.4 新增] 250mAh 容量選項。 */
#define BATTERY_CAPACITY_250_MAH          250U
#define BATTERY_CAPACITY_300_MAH          300U
/* [V1.4.4 新增] 500 / 1000 mAh 共用容量檔位。 */
#define BATTERY_CAPACITY_500_MAH          500U
#define BATTERY_CAPACITY_1000_MAH         1000U

/* [V1.4.4 新增] BLE SET、NVS 及 cfg 共用驗證，先檢查原始 uint24 設定值。 */
constexpr bool brd_battery_capacity_is_supported(uint32_t capacity_mah) {
    return capacity_mah == BATTERY_CAPACITY_200_MAH ||
           capacity_mah == BATTERY_CAPACITY_250_MAH ||
           capacity_mah == BATTERY_CAPACITY_300_MAH ||
           capacity_mah == BATTERY_CAPACITY_500_MAH ||
           capacity_mah == BATTERY_CAPACITY_1000_MAH;
}

#endif
