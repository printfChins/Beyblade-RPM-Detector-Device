/* [新增] 身分標記流式檢查：跨分包標記、一般 BPX 映像拒絕。 */
#include <cassert>
#include <cstdio>
#include <cstring>
#include "ir_test_ota_guard.h"

int main()
{
    IrTestOtaGuard guard;
    const char *tag = IR_TEST_OTA_IMAGE_TAG;
    size_t n = strlen(tag);
    guard.reset();
    for (size_t i = 0; i < n; i++) {
        guard.append(reinterpret_cast<const uint8_t *>(tag + i), 1U);
        assert(guard.valid() == (i + 1U == n));
    }
    guard.reset();
    const char *bp = "BRD_BPX_OTA_IMAGE_V1_C3";
    guard.append(reinterpret_cast<const uint8_t *>(bp), strlen(bp));
    assert(!guard.valid());
    guard.append(reinterpret_cast<const uint8_t *>(tag), n);
    assert(guard.valid());
    puts("PASS: IR_Test image identification and cross-packet recognition");
}
