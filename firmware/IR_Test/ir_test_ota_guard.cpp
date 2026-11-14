/*
 * 檔案：IR_Test_OTA/ir_test_ota_guard.cpp
 * [新增] 流式檢查 IR_Test 韌體標記；可橫跨 BLE 封包邊界。
 */
#include "ir_test_ota_guard.h"

/* [新增] 此常數會由 append() 引用，編譯後會出現在應用程式 BIN。 */
const char IR_TEST_OTA_IMAGE_TAG[] = "BRD_IR_TEST_OTA_IMAGE_V1_C3";

void IrTestOtaGuard::reset()
{
    matched_ = 0U;
    found_ = false;
}

void IrTestOtaGuard::append(const uint8_t *data, size_t length)
{
    if (data == nullptr || found_) {
        return;
    }

    for (size_t i = 0U; i < length; i++) {
        if (data[i] == static_cast<uint8_t>(IR_TEST_OTA_IMAGE_TAG[matched_])) {
            matched_++;
        } else {
            matched_ = (data[i] == static_cast<uint8_t>(IR_TEST_OTA_IMAGE_TAG[0])) ? 1U : 0U;
        }
        if (IR_TEST_OTA_IMAGE_TAG[matched_] == '\0') {
            found_ = true;
            return;
        }
    }
}
