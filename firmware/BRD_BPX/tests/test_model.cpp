/* [新增] 設定模型行為測試：未保存不得套用，CRC 損壞不得載入。 */
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_settings_model.h"
using namespace brd_settings;
static const Values defaults = {0U, 1U, 30U, 2000U, BATTERY_CAPACITY_200_MAH};
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
    assert(blob[4] == 2U && blob[10] == static_cast<uint8_t>(BATTERY_CAPACITY_200_MAH));
    blob[8] ^= 1U;
    assert(!decode_storage(blob, sizeof(blob), 60000U, restored));
    /* [V1.4.3 新增] v1 舊 NVS 使用 200mAh；有 CRC 驗證且不需清除舊設定。 */
    uint8_t legacy[STORAGE_SIZE];
    encode_storage(defaults, legacy);
    legacy[4] = 1U; legacy[10] = 0U; legacy[11] = 0U;
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t index = 0U; index < 12U; index++) {
        crc ^= legacy[index];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320UL : 0U);
        }
    }
    crc = ~crc;
    for (uint8_t i = 0U; i < 4U; i++) { legacy[12U + i] = static_cast<uint8_t>(crc >> (8U * i)); }
    assert(decode_storage(legacy, sizeof(legacy), 60000U, restored));
    assert(restored.battery_capacity_mah == BATTERY_CAPACITY_200_MAH);
    /* [V1.4.3 新增] 300mAh 進行 SET->SAVE，執行值/草稿/GET 旗標一致。 */
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 250U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, BATTERY_CAPACITY_300_MAH) == OK);
    assert(model.active().battery_capacity_mah == BATTERY_CAPACITY_200_MAH);
    assert(model.staged().battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    uint8_t capacity_reply[RESPONSE_SIZE];
    make_response(model, CMD_GET, 19U, 0U, OK, capacity_reply);
    assert((capacity_reply[6] & 0x18U) == 0x10U); /* bit4 草稿 300、bit3 生效 200。 */
    assert(model.request_save(21U) == PENDING);
    model.complete_save(true);
    make_response(model, CMD_GET, 20U, 0U, OK, capacity_reply);
    assert((capacity_reply[6] & 0x18U) == 0x18U);
    encode_storage(model.active(), blob);
    assert(decode_storage(blob, sizeof(blob), 60000U, restored));
    assert(restored.battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    Values unsupported_capacity = model.active();
    unsupported_capacity.battery_capacity_mah = 250U;
    encode_storage(unsupported_capacity, blob);
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
