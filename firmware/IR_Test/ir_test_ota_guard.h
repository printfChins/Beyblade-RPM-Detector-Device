/*
 * 檔案：IR_Test_OTA/ir_test_ota_guard.h
 * [新增] IR_Test 專用 BIN 身分標記檢查，避免誤燒 BRD_BPX 一般韌體。
 * 原本的 BRD_BPX_OTA_IMAGE_V1_C3 標記仍由參考協定檢查，兩者都必須存在。
 */
#ifndef IR_TEST_OTA_GUARD_H
#define IR_TEST_OTA_GUARD_H

#include <stddef.h>
#include <stdint.h>

extern const char IR_TEST_OTA_IMAGE_TAG[];

class IrTestOtaGuard {
public:
    void reset();
    void append(const uint8_t *data, size_t length);
    bool valid() const { return found_; }

private:
    size_t matched_ = 0U;
    bool found_ = false;
};

#endif
