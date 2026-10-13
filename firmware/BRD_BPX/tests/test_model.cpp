/* [新增] 設定模型行為測試：未保存不得套用，CRC 損壞不得載入。 */
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_settings_model.h"
using namespace brd_settings;
static const Values defaults = {0U, 1U, 30U, 2000U};
int main() {
    Manager model(defaults, 60000U);
    assert(model.stage(PARAM_AUTO_MIN_RPM, 3500U) == OK);
    assert(model.active().auto_min_rpm == 2000U);
    assert(model.staged().auto_min_rpm == 3500U);
    assert(model.stage(PARAM_AUTO_MIN_RPM, 0U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_AUTO_MIN_RPM, 60000U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_FINISH_PERCENT, 0U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_FINISH_PERCENT, 101U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_FINISH_PERCENT, 100U) == OK);
    assert(model.stage(PARAM_MODE, 2U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_CHARGING_BLE_OFF, 2U) == OUT_OF_RANGE);
    assert(model.request_save(9U) == PENDING);
    assert(model.stage(PARAM_MODE, 1U) == BUSY);
    model.complete_save(false);
    assert(model.active().auto_min_rpm == 2000U);
    assert(model.last_save_status() == FLASH_ERROR);
    assert(model.request_save(10U) == PENDING);
    model.complete_save(true);
    assert(model.active().auto_min_rpm == 3500U);
    assert(model.last_save_status() == SAVED);
    assert(model.request_save(11U) == SAVED); // 相同內容不重新寫入。
    uint8_t blob[STORAGE_SIZE];
    encode_storage(model.active(), blob);
    Values restored = defaults;
    assert(decode_storage(blob, sizeof(blob), 60000U, restored));
    assert(restored.auto_min_rpm == 3500U && restored.finish_percent == 100U);
    blob[8] ^= 1U;
    assert(!decode_storage(blob, sizeof(blob), 60000U, restored));
    uint8_t request[REQUEST_SIZE] = {0xC1U, 1U, CMD_GET, 17U, 0U, 0U, 0U, 0U};
    uint8_t response[RESPONSE_SIZE];
    assert(handle_request(model, request, sizeof(request), false, response) == OK);
    assert(response[0] == 0xC2U && response[3] == 17U);
    assert(response[8] == 0xACU && response[9] == 0x0DU); // 3500 小端序。
    request[2] = CMD_SET;
    request[4] = PARAM_MODE;
    request[5] = 1U;
    assert(handle_request(model, request, sizeof(request), false, response) == BUSY);
    assert(model.staged().mode == 0U);
    assert(handle_request(model, request, sizeof(request), true, response) == OK);
    assert(model.staged().mode == 1U && model.active().mode == 0U);
    model.discard_staged();
    assert(model.staged().mode == 0U);
    request[1] = 2U;
    assert(handle_request(model, request, sizeof(request), true, response) == BAD_VERSION);
    assert(handle_request(model, request, 2U, true, response) == BAD_LENGTH);
    puts("PASS: settings validation, staging, save failure, CRC and protocol.");
}
