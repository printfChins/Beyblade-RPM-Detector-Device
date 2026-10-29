/*
    檔案位置: BRD_BPX/brd_settings.cpp
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [新增] 開機載入 NVS；設定先暫存，量測結束或閒置才寫入 Flash。
    [新增] 成功寫入並讀回一致後才套用，不在 ISR 或 BLE callback 寫入。
    [新增] 模式切換保留最終 MAX、HOLD 與既有歷史。
*/
#include <Arduino.h>
#include <Preferences.h>
#include "brd_battery.h"
#include "brd_config.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_settings.h"

static const brd_settings::Values g_defaults = {
    BRD_MEASUREMENT_MODE, BLE_OFF_WHILE_CHARGING,
    POST_LAUNCH_FINISH_PERCENT, AUTO_RPM_THRESHOLD
};
static brd_settings::Manager g_model(g_defaults, RPM_VALID_MAX);
static bool g_write_attempted = false;
static uint32_t g_last_write_attempt_ms = 0U;
static bool g_completion_pending = false;
/* [新增] 斷線時 pending snapshot 保留，完成後再丟棄舊連線草稿。 */
static bool g_discard_after_save = false;
static uint8_t g_completion[brd_settings::RESPONSE_SIZE] = {};
static bool g_charge_grace_active = false;
static uint32_t g_charge_grace_start_ms = 0U;

void brd_settings_begin(void) {
    g_model.restore(g_defaults, false);
    g_write_attempted = g_completion_pending = g_charge_grace_active = false;
    g_discard_after_save = false;
    Preferences preferences;
    if (!preferences.begin(SETTINGS_NVS_NAMESPACE, false)) { return; }
    uint8_t blob[brd_settings::STORAGE_SIZE];
    brd_settings::Values restored;
    if (preferences.isKey(SETTINGS_NVS_KEY) &&
        preferences.getBytesLength(SETTINGS_NVS_KEY) == sizeof(blob) &&
        preferences.getBytes(SETTINGS_NVS_KEY, blob, sizeof(blob)) == sizeof(blob) &&
        brd_settings::decode_storage(blob, sizeof(blob), RPM_VALID_MAX, restored)) {
        g_model.restore(restored, true);
    }
    /* [保留] 無資料或資料錯誤使用 cfg 預設值；不自動覆寫無法辨識的 blob。 */
    preferences.end();
}
const brd_settings::Values &brd_settings_get(void) { return g_model.active(); }
const brd_settings::Manager &brd_settings_model(void) { return g_model; }
brd_settings::Status brd_settings_process_request(const uint8_t *data, size_t length,
                                                  uint8_t out[brd_settings::RESPONSE_SIZE]) {
    /* [新增] 主 loop 再檢查一次；入列後若量測開始，修改直接 BUSY，不延後執行。 */
    bool allowed = brd_battery_measurement_allowed() && brd_measurement_modifications_allowed();
    return brd_settings::handle_request(g_model, data, length, allowed, out);
}
void brd_settings_discard_draft(void) {
    /* [新增] 斷線只撤銷未保存草稿；已接受 SAVE 的請求不撤銷。 */
    g_discard_after_save = g_model.discard_staged() == brd_settings::BUSY;
}
void brd_settings_update(void) {
    if (!g_model.pending() || !brd_battery_measurement_allowed()) { return; }
    uint32_t now_ms = millis();
    if (g_write_attempted &&
        static_cast<uint32_t>(now_ms - g_last_write_attempt_ms) < SETTINGS_WRITE_INTERVAL_MS) {
        return;
    }
    /* [新增] 在同一個 critical section 確認安全狀態/空佇列並暫停捕捉。 */
    if (!brd_measurement_pause_for_settings()) { return; }
    g_write_attempted = true;
    g_last_write_attempt_ms = now_ms;
    uint8_t blob[brd_settings::STORAGE_SIZE];
    uint8_t check[brd_settings::STORAGE_SIZE];
    brd_settings::encode_storage(g_model.pending_values(), blob);
    Preferences preferences;
    bool success = preferences.begin(SETTINGS_NVS_NAMESPACE, false);
    if (success) {
        success = preferences.putBytes(SETTINGS_NVS_KEY, blob, sizeof(blob)) == sizeof(blob) &&
                  preferences.getBytesLength(SETTINGS_NVS_KEY) == sizeof(blob) &&
                  preferences.getBytes(SETTINGS_NVS_KEY, check, sizeof(check)) == sizeof(check) &&
                  memcmp(blob, check, sizeof(blob)) == 0;
        preferences.end();
    }
    g_model.complete_save(success);
    if (g_discard_after_save) {
        /* [修正] 失敗也不能讓新連線取得舊連線的未成功草稿。 */
        (void)g_model.discard_staged();
        g_discard_after_save = false;
    }
    brd_measurement_resume_after_settings(success);
    brd_settings::make_response(g_model, brd_settings::CMD_SAVE, g_model.save_sequence(),
                               0U, g_model.last_save_status(), g_completion);
    g_completion_pending = true;
    if (success && g_model.active().charging_ble_off != 0U && brd_io_is_charging()) {
        /* [新增] 已在充電時啟用關閉功能，先留 200ms 傳送保存結果，再關閉 BLE。 */
        g_charge_grace_active = true;
        g_charge_grace_start_ms = millis();
    }
}
bool brd_settings_take_completion(uint8_t out[brd_settings::RESPONSE_SIZE]) {
    if (!g_completion_pending) { return false; }
    memcpy(out, g_completion, sizeof(g_completion));
    g_completion_pending = false;
    return true;
}
bool brd_settings_bluetooth_allowed(bool charging) {
    if (!charging || g_model.active().charging_ble_off == 0U) {
        g_charge_grace_active = false;
        return true;
    }
    if (g_charge_grace_active &&
        static_cast<uint32_t>(millis() - g_charge_grace_start_ms) < SETTINGS_CHARGE_CLOSE_GRACE_MS) {
        return true;
    }
    g_charge_grace_active = false;
    return false;
}
