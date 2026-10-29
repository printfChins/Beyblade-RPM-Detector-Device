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
#include "brd_shot_capture.h"
#include "brd_ui_curve.h"
#include "brd_settings.h"
#include "brd_bbp_session.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
std::map<std::string, std::vector<uint8_t>> test_nvs;
unsigned test_flash_writes = 0U;
bool test_flash_fail = false;
static bool charging = false;
/* [新增] 模擬經 GPIO1 ADC 比較器分類後的邏輯 ACTIVE 狀態。 */
static bool test_load_active = false;
void brd_load_adc_begin() {}
void brd_load_adc_update() {}
bool brd_load_adc_active() { return test_load_active; }
static brd_bbp::Session session;
bool brd_battery_measurement_allowed() { return true; }
bool brd_io_is_charging() { return charging; }
void brd_bbp_capture_reset() { session.reset_capture(); }
void brd_bbp_capture_period(uint32_t period, uint32_t event, uint8_t level) { session.period(period,event,level); }
void brd_bbp_capture_launch(uint32_t event) { session.launched(event); }
void brd_bbp_capture_finish() { session.finish(); }
void brd_bbp_capture_set_max(uint16_t rpm) { session.set_representative_max(rpm); }
void brd_bbp_capture_abort() { session.abort(); }
static void tick(uint32_t us) { test_clock_us += us; brd_measurement_update(); brd_shot::publish_finished(); }
static void edge(uint32_t delta, int level = HIGH) {
    test_clock_us += delta;
    GPIO.in.val = (GPIO.in.val & ~(1U << RPM_IR_GPIO)) | (static_cast<uint32_t>(level) << RPM_IR_GPIO);
    if (test_interrupts[RPM_IR_GPIO] != nullptr) { test_interrupts[RPM_IR_GPIO](); }
    brd_measurement_update();
    brd_shot::publish_finished();
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
    /* [修改] 無既有 NVS 設定時，充電關閉 BLE 的預設功能為停用。 */
    brd_settings_begin();
    assert(brd_settings_get().charging_ble_off == 0U);
    assert(brd_settings_bluetooth_allowed(true));
    fresh_auto();
    // 閒置接受 SAVE 後量測立即開始：Flash 必須延後，當次仍使用舊 30%。
    assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50U) == brd_settings::OK);
    assert(command(brd_settings::CMD_SAVE) == brd_settings::PENDING);
    edge(100U); edge(5000U); // MAX 12000
    assert(!brd_measurement_modifications_allowed());
    assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50U) == brd_settings::BUSY);
    assert(command(brd_settings::CMD_GET) == brd_settings::OK);
    unsigned before = test_flash_writes;
    brd_settings_update();
    assert(test_flash_writes == before);
    edge(10000U); // 6000 > 30%
    assert(!brd_measurement_get_display().show_max);
    edge(20000U); // 3000 < 30%
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 12000U);
    assert(brd_settings_get().finish_percent == 30U);
    tick(1000001U); brd_settings_update();
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
    assert(test_interrupts[LOAD_IR_GPIO] == nullptr); /* [修改] ADC 不掛 GPIO IRQ。 */
    session.update(micros()+BBP_PUBLISH_MIN_DELAY_US,BBP_PUBLISH_MIN_DELAY_US);
    assert(session.state().total == 1U && session.state().lifetime_max == 12000U);
    // 真正 LOAD 流程：裝載去抖後 12000 -> 6000 等於 50%，仍不得結束。
    tick(3000000U);
    test_load_active = true;
    brd_measurement_update(); tick(LOAD_IR_DEBOUNCE_US);
    edge(100U); edge(5000U); edge(10000U);
    assert(!brd_measurement_get_display().show_max);
    edge(12000U); // 5000 < 50%
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 12000U);
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
    edge(100U); edge(40000U); // 1500 < 2000，無效結果。
    tick(AUTO_RESET_ZERO_MS * 1000U + 1U);
    assert(!brd_measurement_get_display().show_max);
    edge(100U); edge(5000U); // 達門檻後無脈衝，共同 300ms 備援。
    tick(RPM_ZERO_TIMEOUT_MS * 1000U + 1U);
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 12000U);
    // 三萬 RPM、負緣先觸發、micros 回繞：32 點滿後 MAX 仍獨立更新。
    fresh_auto();
    test_clock_us = UINT32_MAX - 4095ULL;
    GPIO.in.val |= 1U << RPM_IR_GPIO; // 初始 HIGH 不應影響第一個 LOW 參考。
    edge(100U, LOW);
    edge(5000U, HIGH); // 非參考邊沿不產生 RPM。
    assert(brd_measurement_get_display().value == 0U);
    edge(5000U, LOW); // 6000 RPM，跨越 micros() 回繞。
    for (unsigned index = 1U; index < 40U; index++) {
        edge(5000U, HIGH);
        assert(brd_measurement_get_display().value == 6000U);
        edge(5000U, LOW);
    }
    edge(2000U, LOW); // 曲線已滿，MAX 仍更新到 30000。
    assert(brd_measurement_get_display().value == 30000U);
    edge(10000U, LOW);
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 30000U);
    uint32_t previous_total = session.state().total;
    session.update(micros() + BBP_PUBLISH_MIN_DELAY_US, BBP_PUBLISH_MIN_DELAY_US);
    assert(session.state().total == previous_total + 1U);
    assert(session.state().lifetime_max == 30000U);
    /* [V1.4] 曲線覆蓋整次發射且必須含有後段 MAX 30000 (2000us / 8)。 */
    bool has_peak = false;
    for (unsigned index = 0U; index < brd_bbp::CURVE_SIZE; index++) {
        if (session.state().curve[index] == 250U) { has_peak = true; }
    }
    assert(has_peak);
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
    edge(1000U, HIGH);
    edge(1000U, LOW); // overflow 後只重建同極性基準。
    assert(!brd_measurement_get_display().show_max);
    edge(2000U, LOW);
    assert(brd_measurement_get_display().value == 30000U);
    edge(10000U, LOW);
    assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 30000U);
    /* [修改] 明確啟用後才驗證充電停用，避免測試依賴舊的預設啟用值。 */
    apply(brd_settings::PARAM_CHARGING_BLE_OFF,1U);
    charging = true;
    assert(!brd_settings_bluetooth_allowed(charging));
    apply(brd_settings::PARAM_CHARGING_BLE_OFF,0U);
    assert(brd_settings_bluetooth_allowed(charging));
    puts("PASS: AUTO/LOAD ratio, BUSY/GET, safe Flash, MAX/history, 30000 RPM, polarity, wrap/overflow and charging.");
}
