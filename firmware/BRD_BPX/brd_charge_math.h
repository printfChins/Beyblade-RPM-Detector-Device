/*
    檔案位置：BRD_BPX/brd_charge_math.h
    [V1.5.2 新增] 可自行設定充電電流與容量，每 1% 所需毫秒採 64-bit 計算。
    [說明] 此值為依輸入的實際充電電流所推估的時間，不等同充飽時間；
           CV 階段電流下降及系統耗電未由硬體量測，估算可能偏高。
*/
#ifndef BRD_CHARGE_MATH_H
#define BRD_CHARGE_MATH_H
#include <stdint.h>
constexpr uint32_t brd_charge_one_percent_ms(uint32_t capacity_mah, uint32_t charge_current_ma) {
    return (capacity_mah == 0U || charge_current_ma == 0U) ? 0U :
        static_cast<uint32_t>((36000ULL * capacity_mah + charge_current_ma - 1ULL) / charge_current_ma);
}
#endif
