/* [V1.4.4 修改] 參數/NVS 回歸：v1/v2 遷移、v3 HOLD、三檔容量與 CRC。 */
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_config.h"
#include "brd_settings_model.h"
using namespace brd_settings;
static const Values defaults = {0U, 1U, 30U, 2000U, BATTERY_CAPACITY_200_MAH, OLED_MAX_HOLD_MS};
static void sign_legacy(uint8_t blob[LEGACY_STORAGE_SIZE]) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0U; i < 12U; ++i) {
        crc ^= blob[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
        }
    }
    crc = ~crc;
    for (uint8_t i = 0U; i < 4U; ++i) { blob[12U + i] = static_cast<uint8_t>(crc >> (8U * i)); }
}
int main() {
    Manager model(defaults, 60000U);
    assert(model.stage(PARAM_AUTO_MIN_RPM, 3500U) == OK);
    assert(model.active().auto_min_rpm == 2000U);
    assert(model.stage(PARAM_AUTO_MIN_RPM, 0U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_AUTO_MIN_RPM, 60000U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_FINISH_PERCENT, 0U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_FINISH_PERCENT, 101U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_FINISH_PERCENT, 100U) == OK);
    assert(model.stage(PARAM_MODE, 2U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_CHARGING_BLE_OFF, 2U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_HOLD_MS, HOLD_MIN_MS - 1U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_HOLD_MS, HOLD_MAX_MS + 1U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_HOLD_MS, 4200U) == OK);
    assert(model.request_save(9U) == PENDING);
    assert(model.stage(PARAM_MODE, 1U) == BUSY);
    model.complete_save(false);
    assert(model.active().hold_ms == OLED_MAX_HOLD_MS);
    assert(model.request_save(10U) == PENDING);
    model.complete_save(true);
    assert(model.active().auto_min_rpm == 3500U);
    assert(model.active().hold_ms == 4200U);
    assert(model.request_save(11U) == SAVED);
    uint8_t blob[STORAGE_SIZE];
    encode_storage(model.active(), blob);
    Values restored = defaults;
    assert(decode_storage(blob, sizeof(blob), 60000U, restored));
    assert(restored.auto_min_rpm == 3500U && restored.hold_ms == 4200U);
    assert(blob[4] == 4U && blob[10] == static_cast<uint8_t>(BATTERY_CAPACITY_200_MAH));
    blob[8] ^= 1U;
    assert(!decode_storage(blob, sizeof(blob), 60000U, restored));

    /* [V1.4.4 新增] 模擬舊 v1/v2 完整 16 bytes NVS，保留舊欄位並補 2500ms。 */
    uint8_t legacy[LEGACY_STORAGE_SIZE] = {'B','R','D','C',1U, 0U,1U,30U, 0xD0U,0x07U, 0U,0U};
    sign_legacy(legacy);
    assert(decode_storage(legacy, sizeof(legacy), 60000U, restored));
    assert(restored.battery_capacity_mah == 200U && restored.hold_ms == OLED_MAX_HOLD_MS);
    legacy[4] = 2U; legacy[10] = 0x2CU; legacy[11] = 0x01U;
    sign_legacy(legacy);
    assert(decode_storage(legacy, sizeof(legacy), 60000U, restored));
    assert(restored.battery_capacity_mah == 300U && restored.hold_ms == OLED_MAX_HOLD_MS);
    legacy[10] ^= 1U;
    assert(!decode_storage(legacy, sizeof(legacy), 60000U, restored));

    /* [V1.4.4 修改] 250mAh 允許、非法 225mAh 拒絕、Flags 維持向下相容。 */
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 99U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, BATTERY_CAPACITY_250_MAH) == OK);
    uint8_t reply[RESPONSE_SIZE] = {};
    make_response(model, CMD_GET, 19U, 0U, OK, reply);
    assert((reply[6] & 0x78U) == 0x40U); /* bit6: 草稿 250。 */
    assert(model.request_save(21U) == PENDING);
    model.complete_save(true);
    make_response(model, CMD_GET, 20U, 0U, OK, reply);
    assert((reply[6] & 0x78U) == 0x60U); /* bit5/6: 執行、草稿 250。 */
    assert(model.active().battery_capacity_mah == 250U);
    encode_storage(model.active(), blob);
    assert(decode_storage(blob, sizeof(blob), 60000U, restored));
    assert(restored.battery_capacity_mah == 250U && restored.hold_ms == 4200U);
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 300U) == OK);
    assert(model.request_save(22U) == PENDING);
    model.complete_save(true);
    make_response(model, CMD_GET, 23U, 0U, OK, reply);
    assert((reply[6] & 0x78U) == 0x18U);

    /* [V1.4.4 新增] 新檔位可保存為 v3。混合新舊檔時保留舊容量旗標。 */
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 5001U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 0x0101F4U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 500U) == OK);
    make_response(model, CMD_GET, 24U, 0U, OK, reply);
    assert((reply[6] & 0xF8U) == 0x88U); /* 生效為 300，草稿需查 READ。 */
    assert(model.request_save(25U) == PENDING);
    model.complete_save(true);
    assert(model.active().battery_capacity_mah == 500U);
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 1000U) == OK);
    assert(model.request_save(26U) == PENDING);
    model.complete_save(true);
    make_response(model, CMD_GET, 27U, 0U, OK, reply);
    assert((reply[6] & 0xF8U) == 0x80U);
    encode_storage(model.active(), blob);
    assert(blob[4] == 4U && blob[10] == 0xE8U && blob[11] == 0x03U);
    assert(decode_storage(blob, sizeof(blob), 60000U, restored));
    assert(restored.battery_capacity_mah == 1000U && restored.hold_ms == 4200U);
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 250U) == OK);
    make_response(model, CMD_GET, 28U, 0U, OK, reply);
    assert((reply[6] & 0xF8U) == 0xC0U); /* 生效需查 READ，草稿為 250。 */
    assert(model.request_save(29U) == PENDING);
    model.complete_save(true);
    make_response(model, CMD_GET, 30U, 0U, OK, reply);
    assert((reply[6] & 0xF8U) == 0x60U); /* 切回原檔位後清除 bit7。 */

    /* [V1.5.2 新增] 任意容量／電流／方向都能通過 C1 SET、NVS v4 存取。 */
    assert(model.stage(PARAM_BATTERY_CAPACITY_MAH, 325U) == OK);
    assert(model.stage(PARAM_CHARGE_CURRENT_MA, 180U) == OK);
    assert(model.stage(PARAM_OLED_ROTATION, 1U) == OK);
    assert(model.stage(PARAM_CHARGE_CURRENT_MA, 9U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_CHARGE_CURRENT_MA, 1001U) == OUT_OF_RANGE);
    assert(model.stage(PARAM_OLED_ROTATION, 2U) == OUT_OF_RANGE);
    assert(model.request_save(31U) == PENDING);
    model.complete_save(true);
    encode_storage(model.active(), blob);
    assert(blob[4] == 4U && blob[16] == 180U && blob[17] == 0U && blob[18] == 1U);
    assert(decode_storage(blob, sizeof(blob), 60000U, restored));
    assert(restored.battery_capacity_mah == 325U && restored.charge_current_ma == 180U &&
           restored.oled_rotation == 1U);
    blob[18] ^= 1U;
    assert(!decode_storage(blob, sizeof(blob), 60000U, restored));
    /* [V1.5.2 新增] 構造舊版 v3 20 bytes，確認不遺失容量／HOLD。 */
    uint8_t v3[V3_STORAGE_SIZE] = {'B','R','D','C',3U,0U,1U,30U,0xD0U,0x07U,
                                    0x2CU,0x01U,0xC4U,0x09U,0U,0U};
    uint32_t crc = UINT32_MAX;
    for (size_t i=0U;i<16U;i++) { crc ^= v3[i]; for (uint8_t bit=0U;bit<8U;bit++)
        crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL); }
    crc = ~crc;
    for(uint8_t i=0U;i<4U;i++) v3[16U+i] = static_cast<uint8_t>(crc >> (8U*i));
    assert(decode_storage(v3, sizeof(v3), 60000U, restored));
    assert(restored.battery_capacity_mah == 300U && restored.hold_ms == 2500U &&
           restored.charge_current_ma == 100U && restored.oled_rotation == 0U);
    Values bad = model.active(); bad.hold_ms = HOLD_MAX_MS + 1U;
    encode_storage(bad, blob);
    assert(!decode_storage(blob, sizeof(blob), 60000U, restored));
    uint8_t request[REQUEST_SIZE] = {0xC1U, 1U, CMD_GET, 17U, 0U, 0U, 0U, 0U};
    assert(handle_request(model, request, sizeof(request), false, reply) == OK);
    assert(reply[0] == 0xC2U && reply[3] == 17U);
    assert(reply[8] == 0xACU && reply[9] == 0x0DU);
    request[2] = CMD_SET; request[4] = PARAM_MODE; request[5] = 1U;
    assert(handle_request(model, request, sizeof(request), false, reply) == BUSY);
    assert(handle_request(model, request, sizeof(request), true, reply) == OK);
    assert(model.staged().mode == 1U && model.active().mode == 0U);
    model.discard_staged();
    assert(model.staged().mode == 0U);
    request[1] = 2U;
    assert(handle_request(model, request, sizeof(request), true, reply) == BAD_VERSION);
    assert(handle_request(model, request, 2U, true, reply) == BAD_LENGTH);
    puts("PASS: v1/v2 NVS migration, v4 capacity/current/rotation, CRC and settings protocol.");
}
