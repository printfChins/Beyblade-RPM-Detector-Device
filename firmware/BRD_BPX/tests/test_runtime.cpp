/* [新增] 使用真正量測/設定/NVS adapter；只模擬 GPIO、時間與 Flash 裝置。 */
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_bbp_session.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
std::map<std::string, std::vector<uint8_t>> test_nvs;
unsigned test_flash_writes = 0U;
bool test_flash_fail = false;
static bool charging = false;
static brd_bbp::Session session;
bool brd_battery_measurement_allowed() { return true; }
bool brd_io_is_charging() { return charging; }
void brd_bbp_capture_reset() { session.reset_capture(); }
void brd_bbp_capture_period(uint32_t period, uint32_t event, bool record) { session.period(period,event,record); }
void brd_bbp_capture_launch(uint32_t event) { session.launched(event); }
void brd_bbp_capture_finish() { session.finish(); }
void brd_bbp_capture_abort() { session.abort(); }
static void tick(uint32_t us) { test_clock_us += us; brd_measurement_update(); }
static void edge(uint32_t delta, int level = HIGH) {
    test_clock_us += delta;
    GPIO.in.val = (GPIO.in.val & ~(1U << RPM_IR_GPIO)) | (static_cast<uint32_t>(level) << RPM_IR_GPIO);
    if (test_interrupts[RPM_IR_GPIO] != nullptr) { test_interrupts[RPM_IR_GPIO](); }
    brd_measurement_update();
}
// [V1.4.5 新增] 持續輸入真實相鄰週期，保留原設定／HOLD／回繞／溢位斷言。
static void periods(uint32_t period, unsigned count, int level = HIGH) {
    for (unsigned i = 0U; i < count; ++i) { edge(period, level); }
}
static brd_settings::Status command(uint8_t op, uint8_t param = 0U, uint32_t value = 0U) {
    uint8_t bytes[8] = {0xC1U,1U,op,3U,param,static_cast<uint8_t>(value),
                       static_cast<uint8_t>(value >> 8U),static_cast<uint8_t>(value >> 16U)};
    uint8_t reply[20];
    return brd_settings_process_request(bytes,sizeof(bytes),reply);
}
static void apply(uint8_t param, uint32_t value) {
    assert(command(brd_settings::CMD_SET,param,value) == brd_settings::OK);
    auto result = command(brd_settings::CMD_SAVE);
    assert(result == brd_settings::PENDING || result == brd_settings::SAVED);
    tick(1000001U);
    brd_settings_update();
    assert(!brd_settings_model().pending());
}
static void fresh_auto() {
    brd_measurement_stop();
    brd_settings_begin();
    apply(brd_settings::PARAM_MODE,1U);
    brd_measurement_begin();
}
int main() {
    /* [修改] 無有效 NVS 設定時，容量使用出廠 300 mAh，充電關閉 BLE 預設停用。 */
    brd_settings_begin();
    assert(brd_settings_get().battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    assert(brd_settings_get().charging_ble_off == 0U);
    assert(brd_settings_bluetooth_allowed(true));
    fresh_auto();
    // 閒置接受 SAVE 後量測立即開始：Flash 必須延後，當次仍使用舊 30%。
    assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50U) == brd_settings::OK);
    assert(command(brd_settings::CMD_SAVE) == brd_settings::PENDING);
    edge(100U); periods(5000U, 5U); // 已確認 MAX 12000
    assert(!brd_measurement_modifications_allowed());
    assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50U) == brd_settings::BUSY);
    assert(command(brd_settings::CMD_GET) == brd_settings::OK);
    unsigned before = test_flash_writes;
    brd_settings_update();
    assert(test_flash_writes == before);
    edge(10000U); // 6000 > 30%
    assert(!brd_measurement_get_display().show_max);
    periods(20000U, 4U); // 3000 < 30%，連續確認
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 12000U);
    assert(brd_settings_get().finish_percent == 30U);
    /* [V1.2.3 修改] HOLD 3 秒內不可套用尚未保存的設定，避免影響 MAX 鎖定。 */
    tick(1000001U); brd_settings_update();
    assert(brd_settings_get().finish_percent == 30U);
    tick(OLED_MAX_HOLD_MS * 1000U); brd_settings_update();
    assert(brd_settings_get().finish_percent == 50U);
    // 結束後保存並切換 LOAD，不可清掉凍結的 MAX / Session 曲線。
    assert(command(brd_settings::CMD_SET,brd_settings::PARAM_MODE,0U) == brd_settings::OK);
    assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50U) == brd_settings::OK);
    assert(command(brd_settings::CMD_SAVE) == brd_settings::PENDING);
    brd_settings_update();
    assert(brd_settings_model().pending()); // 距前次寫入未滿一秒，必須等待。
    tick(1000001U);
    brd_settings_update();
    assert(!brd_settings_model().pending());
    assert(brd_settings_get().mode == 0U);
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 12000U);
    assert(test_interrupts[LOAD_IR_GPIO] != nullptr);
    session.update(micros()+BBP_PUBLISH_MIN_DELAY_US,BBP_PUBLISH_MIN_DELAY_US);
    assert(session.state().total == 1U && session.state().lifetime_max == 12000U);
    /* [V1.2.3 修改] 真正 LOAD 流程：達到相等的結束百分比即完成。 */
    tick(3000000U);
    /* [V1.2.2 測試修改] HOLD 已解鎖，先讓實體 LOW 滿去抖時間，才允許下一次 HIGH 裝載。 */
    tick(LOAD_IR_DEBOUNCE_US);
    GPIO.in.val |= 1U << LOAD_IR_GPIO;
    test_interrupts[LOAD_IR_GPIO]();
    brd_measurement_update(); tick(LOAD_IR_DEBOUNCE_US);
    edge(100U); periods(5000U, 5U); periods(10000U, 4U); // 6000 == 50%，確認後 HOLD。
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().value == 12000U);
    edge(12000U); // [V1.2.3 驗證] HOLD 期間 RPM 不得中斷 MAX。
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 12000U);
    tick(LOAD_MAX_HOLD_MS * 1000U + 1000U); // [V1.4.2 修改] LOAD HOLD 2.5 秒後回 WAIT LOAD 才能修改設定。
    // Flash 失敗不切換模式；重啟仍載入最後成功保存值。
    test_flash_fail = true;
    assert(command(brd_settings::CMD_SET,brd_settings::PARAM_MODE,1U) == brd_settings::OK);
    assert(command(brd_settings::CMD_SAVE) == brd_settings::PENDING);
    brd_settings_update();
    assert(brd_settings_get().mode == 0U);
    assert(brd_settings_model().last_save_status() == brd_settings::FLASH_ERROR);
    assert(brd_settings_model().staged().mode == 1U); // 同一連線失敗保留草稿供重試。
    // 已接受 SAVE 跨斷線保留，但失敗後不得把舊連線草稿交給新連線。
    assert(command(brd_settings::CMD_SAVE) == brd_settings::PENDING);
    brd_settings_discard_draft();
    assert(brd_settings_model().pending());
    tick(1000001U); brd_settings_update();
    assert(brd_settings_model().last_save_status() == brd_settings::FLASH_ERROR);
    assert(brd_settings_model().staged().mode == 0U && !brd_settings_model().dirty());
    test_flash_fail = false;
    brd_measurement_stop(); brd_settings_begin();
    assert(brd_settings_get().mode == 0U && brd_settings_get().finish_percent == 50U);
    fresh_auto();
    edge(100U); periods(40000U, 5U); // 1500 < 2000，無效結果。
    tick(AUTO_RESET_ZERO_MS * 1000U + 1U);
    assert(!brd_measurement_get_display().show_max);
    edge(100U); periods(5000U, 5U); // 達門檻後無脈衝，共同 300ms 備援。
    tick(RPM_ZERO_TIMEOUT_MS * 1000U + 1U);
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 12000U);
    // [修改] 單一 FALLING 時戳、micros 回繞：無濾波 MAX，BLE 在停止後均勻抽 32 點。
    fresh_auto();
    test_clock_us = UINT32_MAX - 4095ULL;
    GPIO.in.val |= 1U << RPM_IR_GPIO;
    edge(100U, LOW); // 第一次 FALLING 只記錄時間起點。
    tick(5000U);    // RISING 不呼叫 ISR。
    assert(brd_measurement_get_display().value == 0U);
    edge(5000U, LOW); // 10000us = 6000 RPM，跨越 micros() 回繞。
    assert(brd_measurement_get_display().value == 6000U);
    for (unsigned index = 1U; index < 40U; index++) {
        tick(5000U); // 上升緣不觸發 ISR。
        edge(5000U, LOW);
        assert(brd_measurement_get_display().value == 6000U);
    }
    periods(2000U, 5U, LOW); // 後續峰值 30000 RPM 必須即時更新。
    assert(brd_measurement_get_display().value == 30000U);
    periods(10000U, 4U, LOW);
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 30000U);
    uint32_t previous_total = session.state().total;
    session.update(micros(), 0U);
    assert(session.state().total == previous_total + 1U);
    assert(session.state().lifetime_max == 30000U);
    unsigned nonzero = 0U;
    for (unsigned index = 0U; index < brd_bbp::CURVE_SIZE; index++) {
        if (session.state().curve[index] != 0U) { ++nonzero; }
    }
    assert(nonzero == brd_bbp::CURVE_SIZE);
    // 真正塞滿 RPM ISR queue：不誤結束，也不改選另一極性。
    fresh_auto();
    edge(100U, LOW); edge(10000U, LOW);
    uint32_t overflows = brd_measurement_get_diagnostics().rpm_queue_overflows;
    for (unsigned index = 0U; index < RPM_ISR_QUEUE_SIZE + 5U; index++) {
        test_clock_us += 2000U;
        test_interrupts[RPM_IR_GPIO]();
    }
    brd_measurement_update();
    assert(brd_measurement_get_diagnostics().rpm_queue_overflows == overflows + 1U);
    assert(!brd_measurement_get_display().show_max && brd_measurement_get_display().value == 30000U);
    tick(1000U); // 上升緣不進 ISR。
    edge(1000U, LOW); // overflow 後重建 Falling 時間基準。
    assert(!brd_measurement_get_display().show_max);
    periods(2000U, 5U, LOW);
    assert(brd_measurement_get_display().value == 30000U);
    periods(10000U, 4U, LOW);
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 30000U);
    /* [V1.2.3 修改] HOLD 禁止修改設定；3 秒到期、回 WAIT LOAD 才可執行 CFG。 */
    tick(OLED_MAX_HOLD_MS * 1000U + 1000U);
    /* [修改] 明確啟用後才驗證充電停用，避免測試依賴舊的預設啟用值。 */
    apply(brd_settings::PARAM_CHARGING_BLE_OFF,1U);
    charging = true;
    assert(!brd_settings_bluetooth_allowed(charging));
    apply(brd_settings::PARAM_CHARGING_BLE_OFF,0U);
    assert(brd_settings_bluetooth_allowed(charging));
    /* [修改] 確認各檔跨重啟保存，最後以舊容量 200 mAh 驗證 NVS 優先。 */
    const uint16_t capacities[] = {BATTERY_CAPACITY_300_MAH, BATTERY_CAPACITY_500_MAH,
                                   BATTERY_CAPACITY_1000_MAH, BATTERY_CAPACITY_200_MAH};
    brd_settings::Values expected = brd_settings_get();
    for (uint16_t capacity_mah : capacities) {
        apply(brd_settings::PARAM_BATTERY_CAPACITY_MAH, capacity_mah);
        expected.battery_capacity_mah = capacity_mah;
        assert(brd_settings::equal(brd_settings_get(), expected));
        brd_settings_begin();
        assert(brd_settings::equal(brd_settings_get(), expected));
    }

    /* [新增] DEFAULTS 先載入草稿，SAVE 後才以 300 mAh 生效並持久化。 */
    assert(command(brd_settings::CMD_DEFAULTS) == brd_settings::OK);
    assert(brd_settings_model().staged().battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    assert(brd_settings_get().battery_capacity_mah == BATTERY_CAPACITY_200_MAH);
    assert(command(brd_settings::CMD_SAVE) == brd_settings::PENDING);
    tick(1000001U);
    brd_settings_update();
    assert(brd_settings_model().last_save_status() == brd_settings::SAVED);
    assert(brd_settings_get().battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    brd_settings_begin();
    assert(brd_settings_get().battery_capacity_mah == BATTERY_CAPACITY_300_MAH);
    puts("PASS: AUTO/LOAD, 300mAh default and DEFAULTS/SAVE, NVS battery profiles, RPM/MAX/history and charging.");
}
