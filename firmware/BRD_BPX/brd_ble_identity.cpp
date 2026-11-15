/*
    檔案位置: BRD_BPX/brd_ble_identity.cpp
    [V1.4.4 新增] 允許 BLE 修改廣播名稱，NVS 寫入僅在量測安全窗口進行。
    [新增] 名稱限制 ASCII 英數、底線與連字號，1..20 bytes。
    [新增] NVS 採固定長度完整讀回驗證；無資料時使用預設名稱。
    [保留] 更名不強制斷開現有連線，待斷線後重新啟動廣播。
*/
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "brd_battery.h"
#include "brd_ble_identity.h"
#include "brd_config.h"
#include "brd_measurement.h"

/* [新增] magic 4 bytes + name length 1 byte + max 20 bytes = 25 bytes。 */
static constexpr size_t NAME_BLOB_SIZE = 5U + BBP_DEVICE_NAME_MAX_LENGTH;
static portMUX_TYPE g_identity_mux = portMUX_INITIALIZER_UNLOCKED;
static char g_active_name[BBP_DEVICE_NAME_MAX_LENGTH + 1U] = BBP_DEVICE_NAME_DEFAULT;
static char g_pending_name[BBP_DEVICE_NAME_MAX_LENGTH + 1U] = {};
static uint8_t g_pending_length = 0U;
static bool g_name_pending = false;
static bool g_refresh_advertising = false;
static brd_ble_name_status_t g_status = BRD_BLE_NAME_IDLE;

static bool valid_name(const uint8_t *name, size_t length) {
    if (name == nullptr || length == 0U || length > BBP_DEVICE_NAME_MAX_LENGTH) {
        return false;
    }
    for (size_t i = 0U; i < length; ++i) {
        uint8_t ch = name[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
            return false;
        }
    }
    return true;
}

static void make_blob(const char *name, uint8_t length, uint8_t out[NAME_BLOB_SIZE]) {
    memset(out, 0, NAME_BLOB_SIZE);
    out[0] = 'B'; out[1] = 'R'; out[2] = 'D'; out[3] = 'N';
    out[4] = length;
    memcpy(out + 5U, name, length);
}

void brd_ble_identity_begin(void) {
    portENTER_CRITICAL(&g_identity_mux);
    memcpy(g_active_name, BBP_DEVICE_NAME_DEFAULT, sizeof(BBP_DEVICE_NAME_DEFAULT));
    g_name_pending = g_refresh_advertising = false;
    g_status = BRD_BLE_NAME_IDLE;
    portEXIT_CRITICAL(&g_identity_mux);
    Preferences preferences;
    if (!preferences.begin(BBP_IDENTITY_NVS_NAMESPACE, true)) { return; }
    uint8_t blob[NAME_BLOB_SIZE] = {};
    if (preferences.isKey(BBP_IDENTITY_NVS_KEY) &&
        preferences.getBytesLength(BBP_IDENTITY_NVS_KEY) == sizeof(blob) &&
        preferences.getBytes(BBP_IDENTITY_NVS_KEY, blob, sizeof(blob)) == sizeof(blob) &&
        blob[0] == 'B' && blob[1] == 'R' && blob[2] == 'D' && blob[3] == 'N' &&
        valid_name(blob + 5U, blob[4])) {
        /* [新增] 無效 NVS 保持出廠名稱；合法值才取代。 */
        char restored[BBP_DEVICE_NAME_MAX_LENGTH + 1U] = {};
        memcpy(restored, blob + 5U, blob[4]);
        portENTER_CRITICAL(&g_identity_mux);
        memcpy(g_active_name, restored, sizeof(g_active_name));
        portEXIT_CRITICAL(&g_identity_mux);
    }
    preferences.end();
}

const char *brd_ble_identity_active_name(void) {
    /* [保留] 主 loop 唯一呼叫 BLE begin，此回傳不應由 ISR 使用。 */
    return g_active_name;
}

void brd_ble_identity_read_name(char *out, size_t capacity) {
    if (out == nullptr || capacity == 0U) { return; }
    portENTER_CRITICAL(&g_identity_mux);
    size_t len = strnlen(g_active_name, BBP_DEVICE_NAME_MAX_LENGTH);
    if (len >= capacity) { len = capacity - 1U; }
    memcpy(out, g_active_name, len);
    out[len] = '\0';
    portEXIT_CRITICAL(&g_identity_mux);
}

brd_ble_name_status_t brd_ble_identity_request_name(const uint8_t *name, size_t length) {
    /* [新增] 在 BLE callback 僅做驗證與 RAM 暫存，不中斷 RPM 捕捉。 */
    /* [V1.4.4 修正] 已接受的待保存更名不得被後續非法/忙碌請求改寫狀態。 */
    portENTER_CRITICAL(&g_identity_mux);
    bool already_pending = g_name_pending;
    portEXIT_CRITICAL(&g_identity_mux);
    if (already_pending) { return BRD_BLE_NAME_BUSY; }
    if (!valid_name(name, length)) {
        portENTER_CRITICAL(&g_identity_mux);
        g_status = BRD_BLE_NAME_INVALID;
        portEXIT_CRITICAL(&g_identity_mux);
        return BRD_BLE_NAME_INVALID;
    }
    if (!brd_battery_measurement_allowed() || !brd_measurement_modifications_allowed()) {
        portENTER_CRITICAL(&g_identity_mux);
        g_status = BRD_BLE_NAME_BUSY;
        portEXIT_CRITICAL(&g_identity_mux);
        return BRD_BLE_NAME_BUSY;
    }
    portENTER_CRITICAL(&g_identity_mux);
    if (g_name_pending) {
        /* [新增] 只允許單筆待存名稱；不能覆蓋已被接受的交易。 */
        portEXIT_CRITICAL(&g_identity_mux);
        return BRD_BLE_NAME_BUSY;
    }
    memcpy(g_pending_name, name, length);
    g_pending_name[length] = '\0';
    g_pending_length = static_cast<uint8_t>(length);
    g_name_pending = true;
    g_status = BRD_BLE_NAME_PENDING;
    portEXIT_CRITICAL(&g_identity_mux);
    return BRD_BLE_NAME_PENDING;
}

brd_ble_name_status_t brd_ble_identity_status(void) {
    portENTER_CRITICAL(&g_identity_mux);
    brd_ble_name_status_t status = g_status;
    portEXIT_CRITICAL(&g_identity_mux);
    return status;
}

void brd_ble_identity_update(void) {
    /* [新增] 量測中禁止 NVS；SAFE 後仍須再次確認 GPIO 事件佇列。 */
    if (!brd_battery_measurement_allowed() || !brd_measurement_modifications_allowed()) {
        return;
    }
    char requested[BBP_DEVICE_NAME_MAX_LENGTH + 1U] = {};
    uint8_t length = 0U;
    portENTER_CRITICAL(&g_identity_mux);
    if (!g_name_pending) {
        portEXIT_CRITICAL(&g_identity_mux);
        return;
    }
    length = g_pending_length;
    memcpy(requested, g_pending_name, sizeof(requested));
    portEXIT_CRITICAL(&g_identity_mux);
    if (strcmp(requested, g_active_name) == 0) {
        portENTER_CRITICAL(&g_identity_mux);
        g_name_pending = false;
        g_status = BRD_BLE_NAME_SAVED;
        portEXIT_CRITICAL(&g_identity_mux);
        return;
    }
    if (!brd_measurement_pause_for_settings()) { return; }
    uint8_t blob[NAME_BLOB_SIZE], check[NAME_BLOB_SIZE];
    make_blob(requested, length, blob);
    Preferences preferences;
    bool success = preferences.begin(BBP_IDENTITY_NVS_NAMESPACE, false);
    if (success) {
        success = preferences.putBytes(BBP_IDENTITY_NVS_KEY, blob, sizeof(blob)) == sizeof(blob) &&
                  preferences.getBytesLength(BBP_IDENTITY_NVS_KEY) == sizeof(blob) &&
                  preferences.getBytes(BBP_IDENTITY_NVS_KEY, check, sizeof(check)) == sizeof(check) &&
                  memcmp(blob, check, sizeof(blob)) == 0;
        preferences.end();
    }
    portENTER_CRITICAL(&g_identity_mux);
    g_name_pending = false;
    if (success) {
        memcpy(g_active_name, requested, sizeof(g_active_name));
        g_status = BRD_BLE_NAME_SAVED;
        g_refresh_advertising = true;
    } else {
        g_status = BRD_BLE_NAME_FLASH_ERROR;
    }
    portEXIT_CRITICAL(&g_identity_mux);
    brd_measurement_resume_after_settings(false);
}

bool brd_ble_identity_needs_advertising_refresh(void) {
    portENTER_CRITICAL(&g_identity_mux);
    bool refresh = g_refresh_advertising;
    portEXIT_CRITICAL(&g_identity_mux);
    return refresh;
}

void brd_ble_identity_clear_advertising_refresh(void) {
    portENTER_CRITICAL(&g_identity_mux);
    g_refresh_advertising = false;
    portEXIT_CRITICAL(&g_identity_mux);
}
