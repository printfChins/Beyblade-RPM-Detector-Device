// 審查測試用。連結原始量測、BBP、設定與名稱模組，GPIO、時鐘、BLE 和 NVS 使用主機替身。
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <soc/gpio_struct.h>
#include "brd_bbp.h"
#include "brd_ble_identity.h"
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"

uint64_t test_clock_us = 1000000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
std::map<std::string, std::vector<uint8_t>> test_nvs;
unsigned test_flash_writes = 0U;
bool test_flash_fail = false;
bool test_notify_ok = true;
static bool battery_allowed = true;
static NimBLEConnInfo peer(7U);
static NimBLECharacteristic *bbp = nullptr;
/* [本次修改] 單一固定下降緣 (LOW)，不再以參數選擇上升緣參考。 */
static constexpr int reference_level = LOW;

bool brd_battery_measurement_allowed() { return battery_allowed; }
uint8_t brd_battery_get_percent() { return battery_allowed ? 80U : 4U; }
bool brd_io_is_charging() { return false; }
bool brd_settings_ble_create(NimBLEServer *) { return true; }
void brd_settings_ble_connected(uint16_t) {}
void brd_settings_ble_disconnected(uint16_t) {}
void brd_settings_ble_stop() {}
bool brd_ota_create(NimBLEServer *) { return true; }
void brd_ota_connected(uint16_t) {}
void brd_ota_disconnected(uint16_t) {}
void brd_ota_stop() {}

static void poll() {
    if (!brd_battery_measurement_allowed()) {
        brd_measurement_stop();
        brd_bbp_stop();
        return;
    }
    brd_measurement_begin();
    brd_measurement_update();
    const bool storage_allowed = brd_measurement_storage_allowed();
    if (storage_allowed) { brd_bbp_begin(); }
    brd_bbp_update(storage_allowed);
    brd_settings_update();
}

static void advance(uint32_t us) {
    test_clock_us += us;
    poll();
}

static void pump(uint32_t us) {
    assert(us % 10000U == 0U);
    for (uint32_t i = 0U; i < us; i += 10000U) { advance(10000U); }
}

static void edge(int pin, int level, uint32_t us) {
    test_clock_us += us;
    assert(((GPIO.in.val >> pin) & 1U) != static_cast<uint32_t>(level));
    GPIO.in.val = (GPIO.in.val & ~(1U << pin)) | (static_cast<uint32_t>(level) << pin);
    assert(test_interrupts[pin] != nullptr);
    /* [本次修改] 主機依 FALLING/CHANGE 模擬硬體觸發；上升緣不可呼叫 RPM ISR。 */
    if (pin != RPM_IR_GPIO || level == LOW) { test_interrupts[pin](); }
    poll();
}

static void rpm_period(uint32_t us) {
    edge(RPM_IR_GPIO, 1 - reference_level, us / 2U);
    edge(RPM_IR_GPIO, reference_level, us - us / 2U);
}

static brd_settings::Status setting_command(uint8_t op, uint8_t param = 0U, uint32_t value = 0U) {
    const uint8_t bytes[8] = {0xC1U, 1U, op, 3U, param, static_cast<uint8_t>(value),
                             static_cast<uint8_t>(value >> 8U), static_cast<uint8_t>(value >> 16U)};
    uint8_t reply[20];
    return brd_settings_process_request(bytes, sizeof(bytes), reply);
}

static void save_setting(uint8_t parameter, uint32_t value) {
    assert(setting_command(brd_settings::CMD_SET, parameter, value) == brd_settings::OK);
    assert(setting_command(brd_settings::CMD_SAVE) == brd_settings::PENDING);
    brd_settings_update();
    uint8_t reply[20];
    assert(brd_settings_take_completion(reply));
    assert(brd_settings_model().last_save_status() == brd_settings::SAVED);
}

static void connect() {
    auto *server = NimBLEDevice::getServer();
    assert(server != nullptr);
    server->callbacks->onConnect(server, peer);
    bbp = server->getService(BBP_SERVICE_UUID)->getCharacteristic(BBP_CHARACTERISTIC_UUID);
    bbp->getCallbacks()->onSubscribe(bbp, peer, 1U);
    pump(200000U);
    assert(brd_bbp_is_connected());
}

static void setup(bool auto_mode) {
    brd_settings_begin();
    brd_ble_identity_begin();
    if (auto_mode) { save_setting(brd_settings::PARAM_MODE, BRD_MEASUREMENT_MODE_AUTO); }
    assert(brd_bbp_begin());
    brd_measurement_begin();
    connect();
    if (!auto_mode) {
        edge(LOAD_IR_GPIO, HIGH, 1000U);
        advance(LOAD_IR_DEBOUNCE_US);
        assert(brd_measurement_get_display().loaded);
    }
    assert(brd_measurement_modifications_allowed());
}

static void bbp_command(uint8_t command) {
    bbp->setValue(&command, 1U);
    bbp->getCallbacks()->onWrite(bbp, peer);
}

static unsigned get_u16(const std::vector<uint8_t> &page, unsigned start) {
    return static_cast<unsigned>(page.at(start)) | (static_cast<unsigned>(page.at(start + 1U)) << 8U);
}

static unsigned history_count() {
    bbp->sent.clear();
    bbp_command(0x51U);
    pump(200000U);
    for (auto it = bbp->sent.rbegin(); it != bbp->sent.rend(); ++it) {
        if (it->at(0) == 0xA0U) { return get_u16(*it, 9U); }
    }
    assert(false);
    return 0U;
}

static unsigned first_curve_point() {
    bbp->sent.clear();
    bbp_command(0x74U);
    pump(200000U);
    for (const auto &page : bbp->sent) {
        if (page.at(0) == 0x70U) { return get_u16(page, 1U); }
    }
    assert(false);
    return 0U;
}


/* [V1.4.5 新增] 17-byte 曲線頁每頁八點；不可殘留代表 >12000 RPM 的短週期。 */
static void verify_no_fast_curve() {
    bbp->sent.clear();
    bbp_command(0x74U);
    pump(200000U);
    unsigned points = 0U;
    for (const auto &page : bbp->sent) {
        if (page[0] >= 0x70U && page[0] <= 0x73U) {
            for (unsigned index = 1U; index < 17U; index += 2U) {
                const unsigned value = get_u16(page, index);
                if (value != 0U) { assert(value >= 625U); ++points; }
            }
        }
    }
    assert(points != 0U);
}

/* [V1.4.5 新增] 實際 GPIO 邊沿進入 ISR、量測及 BBP，數值由已知波形手算。 */
static void begin_pulses(unsigned count = 5U) {
    if (reference_level == LOW) { GPIO.in.val |= 1U << RPM_IR_GPIO; }
    edge(RPM_IR_GPIO, reference_level, 100U);
    for (unsigned i = 0U; i < count; ++i) { rpm_period(5000U); }
}
static void finish_and_check(unsigned expected_max = 12000U, unsigned expected_total = 1U) {
    for (unsigned i = 0U; i < 6U; ++i) { rpm_period(40000U); }
    assert(brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().value == expected_max);
    pump(700000U);
    assert(history_count() == expected_total);
    assert(first_curve_point() != 0U);
}
int main(int argc, char **argv) {
    assert(argc >= 2);
    const std::string scenario = argv[1];
    const bool automatic = argc > 2 && std::string(argv[2]) == "auto";
    /* [本次刪減] 正／負緣模式切換，所有案例一律使用 Falling -> Falling。 */
    setup(automatic);
    if (scenario == "rising-only") {
        /* [本次新增] 僅有 HIGH 的訊號不得觸發硬體 FALLING ISR，也不得啟動 SP 或曲線。 */
        for (unsigned i = 0U; i < 12U; ++i) {
            GPIO.in.val &= ~(1U << RPM_IR_GPIO);
            edge(RPM_IR_GPIO, HIGH, 5000U);
        }
        assert(brd_measurement_get_display().value == 0U);
        assert(!brd_measurement_get_display().show_max);
        assert(brd_measurement_modifications_allowed());
    } else if (scenario == "clear-ready") {
        bbp_command(0x75U);
        pump(200000U);
        assert(brd_measurement_modifications_allowed());
        begin_pulses();
        finish_and_check();
        assert(first_curve_point() == 625U);
    } else if (scenario == "clear-spinning") {
        begin_pulses();
        bbp_command(0x75U);
        poll();
        assert(brd_bbp_get_diagnostics().invalid_commands == 1U);
        finish_and_check();
    } else if (scenario == "clear-hold") {
        begin_pulses();
        for (unsigned i = 0; i < 4U; ++i) { rpm_period(40000U); }
        assert(brd_measurement_get_display().hold_active);
        bbp_command(0x75U);
        poll();
        assert(brd_bbp_get_diagnostics().invalid_commands == 1U);
        pump(700000U);
        assert(history_count() == 1U);
    } else if (scenario == "single-low") {
        begin_pulses();
        rpm_period(25000U);
        assert(!brd_measurement_get_display().hold_active);
        for (unsigned i = 0; i < 5U; ++i) { rpm_period(5000U); }
        assert(!brd_measurement_get_display().hold_active);
        finish_and_check();
    } else if (scenario == "noise" || scenario == "startup-noise") {
        const unsigned phase = argc > 4 ? static_cast<unsigned>(std::stoul(argv[4])) : 1100U;
        begin_pulses(scenario == "noise" ? 5U : 0U);
        rpm_period(phase);
        rpm_period(5000U - phase);
        assert(!brd_measurement_get_display().hold_active);
        assert(brd_measurement_get_display().value <= 12000U);
        for (unsigned i = 0; i < 6U; ++i) {
            rpm_period(5000U);
            assert(!brd_measurement_get_display().hold_active);
            assert(brd_measurement_get_display().value <= 12000U);
        }
        finish_and_check();
        verify_no_fast_curve();
    } else if (scenario == "high-after-32") {
        begin_pulses(40U);
        for (unsigned i = 0; i < 5U; ++i) { rpm_period(2000U); }
        assert(brd_measurement_get_display().value == 30000U);
        finish_and_check(30000U);
        assert(first_curve_point() == 625U);
    } else if (scenario == "wrap-zero") {
        test_clock_us = UINT32_MAX - 4095ULL;
        begin_pulses();
        advance(RPM_ZERO_TIMEOUT_MS * 1000U - 1U);
        assert(!brd_measurement_get_display().hold_active);
        advance(1U);
        assert(brd_measurement_get_display().hold_active);
        assert(brd_measurement_get_display().value == 12000U);
        pump(700000U);
        assert(history_count() == 1U);
    } else if (scenario == "overflow-single-low") {
        begin_pulses();
        const uint32_t previous = brd_measurement_get_diagnostics().rpm_queue_overflows;
        // [V1.4.5 新增] 主迴圈暫停期間塞滿 ISR queue，保持真實正負電位切換。
        for (unsigned i = 0U; i < RPM_ISR_QUEUE_SIZE + 5U; ++i) {
            for (int level : {1 - reference_level, reference_level}) {
                test_clock_us += 2500U;
                GPIO.in.val = (GPIO.in.val & ~(1U << RPM_IR_GPIO)) |
                              (static_cast<uint32_t>(level) << RPM_IR_GPIO);
                /* [本次修改] 溢位測試也只送下降緣，模擬真實中斷模式。 */
                if (level == LOW) { test_interrupts[RPM_IR_GPIO](); }
            }
        }
        poll();
        assert(brd_measurement_get_diagnostics().rpm_queue_overflows == previous + 1U);
        assert(!brd_measurement_get_display().hold_active);
        rpm_period(5000U); // 缺失事件之後重新建立參考基準。
        rpm_period(25000U); // 唯一一筆低值；不得在起始窗口被重複當成兩筆確認。
        for (unsigned i = 0U; i < 5U; ++i) {
            rpm_period(5000U);
            assert(!brd_measurement_get_display().hold_active);
        }
        finish_and_check();
    } else if (scenario == "startup-short") {
        begin_pulses(2U);
        advance(RPM_ZERO_TIMEOUT_MS * 1000U);
        assert(!brd_measurement_get_display().hold_active);
        assert(brd_measurement_get_display().value == 0U);
        assert(history_count() == 0U);
    } else if (scenario == "startup-three") {
        begin_pulses(3U);
        advance(RPM_ZERO_TIMEOUT_MS * 1000U);
        assert(brd_measurement_get_display().hold_active);
        assert(brd_measurement_get_display().value == 12000U);
        pump(700000U);
        assert(history_count() == 1U);
    } else {
        assert(false);
    }
    printf("PASS: %s %s %s\n", scenario.c_str(), automatic ? "AUTO" : "LOAD",
           "falling");
}
