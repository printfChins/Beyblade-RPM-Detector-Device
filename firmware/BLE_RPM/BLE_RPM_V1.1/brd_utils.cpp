/*
    檔案名稱: brd_utils.cpp
    檔案位置: BLE_RPM_V1.0/brd_utils.cpp

    通用輔助函式。
*/

#include "brd_utils.h"

uint16_t brd_limit_u32_to_u16(uint32_t value) {
    if (value > 65535UL) {
        return 65535U;
    }

    return (uint16_t)value;
}

const char *brd_log_level_to_text(int level) {
    if (level == HIGH) {
        return "HIGH";
    }

    if (level == LOW) {
        return "LOW";
    }

    return "UNKNOWN";
}

const char *brd_state_to_text(brd_state_t state) {
    switch (state) {
        case BRD_STATE_WAIT_LOAD:
            return "WAIT_LOAD";

        case BRD_STATE_LOADED_READY:
            return "LOADED_READY";

        case BRD_STATE_SPINNING_LOADED:
            return "SPINNING_LOADED";

        case BRD_STATE_SPINNING_LAUNCHED:
            return "SPINNING_LAUNCHED";

        case BRD_STATE_RESULT_PENDING:
            return "RESULT_PENDING";

        default:
            return "UNKNOWN";
    }
}

void brd_write_u16_le(uint8_t *buf, uint16_t value) {
    if (buf == nullptr) {
        return;
    }

    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);
}
