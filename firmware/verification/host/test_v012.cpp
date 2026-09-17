/* [V0.12 新增] 主機回歸測試；直接載入交付原始碼，替代硬體介面。 */
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <Arduino.h>
#include <soc/gpio_struct.h>

uint64_t test_us = 0;
void (*callbacks[32])() = {};
int test_pin_modes[32] = {};
int test_pull_modes[32] = {};
int test_gpio_error = 0;
int test_map_error = 0, test_new_error = 0, test_config_error = 0, test_read_error = 0;
int test_adc_raw = 2000, test_adc_mv = 1850;
int test_adc_reads = 0, test_adc_units = 0, test_adc_configs = 0;
int test_cali_error = 0, test_cali_create_error = 0;
int test_i2c_error = 0, test_i2c_count = 0;
int test_restarts = 0;
int mock_reset_reason = 1;
// [V1.10 新增] 模擬 ESP32-C3 Deep-sleep 入口，避免主機測試停止執行。
int mock_sleep_error = 0, mock_sleep_calls = 0;
uint64_t mock_sleep_wake_mask = 0ULL;
gpio_mock GPIO = {};

#include "../../BRD_BLE_OLED/brd_record.cpp"
#include "../../BRD_BLE_OLED/brd_ble.cpp"
#include "../../BRD_BLE_OLED/brd_measurement.cpp"
#include "../../BRD_BLE_OLED/brd_battery.cpp"
#include "../../BRD_BLE_OLED/brd_io.cpp"
#include "../../BRD_BLE_OLED/brd_oled.cpp"
#include "../../BRD_BLE_OLED/brd_power.cpp"
#include "../../BRD_BLE_OLED/BRD_BLE_OLED.ino"

static void at(uint64_t us) {
    assert(us >= test_us);
    test_us = us;
}
static void update_at(uint64_t us) {
    at(us);
    brd_measurement_update();
}
static void load_at(uint64_t us, bool high) {
    at(us);
    if (high) GPIO.in.val |= 1U << LOAD_IR_GPIO;
    else GPIO.in.val &= ~(1U << LOAD_IR_GPIO);
    if (callbacks[LOAD_IR_GPIO]) callbacks[LOAD_IR_GPIO]();
}
static void pulse_at(uint64_t us, bool update = true) {
    at(us);
    if (callbacks[RPM_IR_GPIO]) callbacks[RPM_IR_GPIO]();
    if (update) brd_measurement_update();
}
static void rpm_edge_at(uint64_t us, bool high, bool update = true) {
    at(us);
    if (high) GPIO.in.val |= 1U << RPM_IR_GPIO;
    else GPIO.in.val &= ~(1U << RPM_IR_GPIO);
    if (callbacks[RPM_IR_GPIO]) callbacks[RPM_IR_GPIO]();
    if (update) brd_measurement_update();
}
static void loaded(uint64_t base = 0) {
    at(base);
    GPIO.in.val |= 1U << LOAD_IR_GPIO;
    brd_measurement_begin();
    update_at(base + 1000);
    assert(brd_measurement_get_display().loaded);
}
static void baseline(uint64_t base = 0) {
    loaded(base);
    pulse_at(base + 3000); pulse_at(base + 6000); pulse_at(base + 9000);
    assert(brd_measurement_get_display().value == 20000);
}
static void launch(uint64_t base = 0) {
    load_at(base + 9500, false);
    update_at(base + 10500);
    assert(!brd_measurement_get_display().loaded);
}
static void battery_at(uint64_t us, int mv, int error = ESP_OK) {
    at(us);
    test_adc_mv = mv;
    test_read_error = error;
    brd_battery_update();
}
static void dump_frame(const char *name) {
    std::ofstream file(name, std::ios::binary);
    file << "P5\n128 32\n255\n";
    for (unsigned y = 0; y < 32; y++) {
        for (unsigned x = 0; x < 128; x++) {
            unsigned char pixel = (g_oled_buffer[x + (y / 8) * 128] & (1U << (y % 8))) ? 255 : 0;
            file.write(reinterpret_cast<const char *>(&pixel), 1);
        }
    }
}

int main(int argc, char **argv) {
    assert(argc == 2);
    std::string name = argv[1];
    if (name == "rpm_dual_edge_change_trigger") {
        assert(RPM_IR_TRIGGER_EDGE == CHANGE);
    } else if (name == "rpm_dual_edge_full_revolution") {
        loaded();
        /*
            50% 編碼盤的光學門檻實際可能不是精準 180/180。
            這裡刻意使用 1200 us / 1300 us 半圈，完整一圈仍為 2500 us。
            正確結果應固定為 24000 RPM，不得直接用相鄰半圈算出 25000 / 23076 RPM。
        */
        rpm_edge_at(3000, false);
        rpm_edge_at(4200, true);
        rpm_edge_at(5500, false);
        assert(brd_measurement_get_display().value == 24000);
        rpm_edge_at(6700, true);
        assert(brd_measurement_get_display().value == 24000);
    } else if (name == "rpm_dual_edge_half_rev_update") {
        loaded();
        /* Falling 完整圈 2600 us -> 23076 RPM。 */
        rpm_edge_at(3000, false);
        rpm_edge_at(4500, true);
        rpm_edge_at(5600, false);
        assert(brd_measurement_get_display().value == 23076);
        /* Rising 完整圈 2400 us -> 25000 RPM，只過約半圈就應更新。 */
        rpm_edge_at(6900, true);
        assert(brd_measurement_get_display().value == 25000);
        assert(brd_measurement_get_telemetry().max_rpm == 25000);
    } else if (name == "rpm_dual_edge_micros_wrap") {
        uint64_t base = (1ULL << 32) - 5000;
        loaded(base);
        rpm_edge_at(base + 1500, false);
        rpm_edge_at(base + 2700, true);
        rpm_edge_at(base + 4000, false);
        assert(brd_measurement_get_display().value == 24000);
        /* Rising 的完整一圈跨越 micros() 32-bit 回繞，仍應為 2500 us。 */
        rpm_edge_at(base + 5200, true);
        assert(brd_measurement_get_display().value == 24000);
    } else if (name == "launch_uses_latest_dual_edge_rpm") {
        loaded();
        rpm_edge_at(3000, false);
        rpm_edge_at(4300, true);
        rpm_edge_at(6000, false);   // Falling full revolution = 3000 us -> 20000 RPM
        rpm_edge_at(6800, true);    // Rising full revolution = 2500 us -> 24000 RPM
        load_at(7000, false);
        update_at(8000);
        auto r = brd_record_get_info();
        assert(r.launch_valid);
        assert(r.launch_rpm == 24000);
        assert(r.max_at_launch == 24000);
    } else if (name == "threshold35") {
        baseline(); launch();
        // [V1.11 測試] MAX=20000，7000 RPM 為 35%；6666 RPM 必須觸發停止。
        pulse_at(18000);
        auto d = brd_measurement_get_display();
        assert(d.show_max && d.hold_active && d.value == 20000);
    } else if (name == "one_dropped_edge") {
        baseline(); launch(); pulse_at(15000);
        assert(!brd_measurement_get_display().show_max && brd_measurement_get_display().value == 10000);
        pulse_at(18000);
        assert(brd_measurement_get_display().value == 20000);
    } else if (name == "spike_policy_unchanged") {
        baseline(); pulse_at(10000); pulse_at(12000);
        load_at(12500, false); update_at(13500); pulse_at(14500);
        assert(!brd_measurement_get_display().show_max);
        pulse_at(45000);
        assert(brd_measurement_get_display().show_max && brd_measurement_get_display().value == 60000);
    } else if (name == "load_backlog") {
        baseline();
        load_at(10000, false); load_at(12000, true);
        pulse_at(12000, false); pulse_at(15000, false); update_at(16000);
        auto diag = brd_measurement_get_diagnostics();
        assert(diag.launch_events == 1 && diag.load_stable_transitions == 3);
        assert(brd_measurement_get_display().loaded && brd_measurement_get_display().value == 0);
    } else if (name == "load_short_bounce") {
        baseline();
        load_at(10000, false); load_at(10999, true); update_at(15000);
        assert(brd_measurement_get_diagnostics().launch_events == 0);
        assert(brd_measurement_get_display().value == 20000);
    } else if (name == "load_exact_debounce") {
        baseline(); load_at(10000, false); load_at(11000, true); update_at(13000);
        assert(brd_measurement_get_diagnostics().launch_events == 1);
        assert(brd_measurement_get_display().loaded && brd_measurement_get_display().value == 0);
    } else if (name == "load_rpm_chronology") {
        baseline();
        load_at(10000, false);
        pulse_at(12000, false); pulse_at(27000, false);
        load_at(28000, true); update_at(31000);
        assert(brd_measurement_get_display().hold_active && brd_measurement_get_display().value == 20000);
        assert(brd_measurement_get_diagnostics().load_stable_transitions == 2);
    } else if (name == "timeout_chronology") {
        baseline(); load_at(10000, false); load_at(400000, true); update_at(405000);
        assert(brd_measurement_get_display().hold_active);
        assert(brd_measurement_get_display().value == 20000);
    } else if (name == "unload_before_first_rpm") {
        loaded(); load_at(5000, false); pulse_at(5200, false); pulse_at(8000, false); update_at(9000);
        assert(!brd_measurement_get_display().show_max && brd_measurement_get_display().value == 0);
        assert(brd_measurement_get_diagnostics().launch_events == 0);
    } else if (name == "load_overflow") {
        baseline();
        for (uint64_t i = 0; i < LOAD_ISR_QUEUE_SIZE + 8; i++) load_at(10000 + i * 50, (i % 2) != 0);
        update_at(15000);
        assert(brd_measurement_get_diagnostics().load_queue_overflows == 1);
        assert(!brd_measurement_get_display().show_max && brd_measurement_get_display().value == 0);
        update_at(15999); assert(!brd_measurement_get_display().loaded);
        update_at(16000); assert(brd_measurement_get_display().loaded);
        pulse_at(18000); pulse_at(21000);
        assert(brd_measurement_get_display().value == 20000);
    } else if (name == "rpm_overflow") {
        baseline();
        for (uint64_t i = 0; i < RPM_ISR_QUEUE_SIZE + 4; i++) pulse_at(10000 + i * 100, false);
        update_at(24000);
        assert(brd_measurement_get_diagnostics().rpm_queue_overflows == 1);
        assert(brd_measurement_get_display().value == 0);
        pulse_at(27000); assert(brd_measurement_get_display().value == 0);
        pulse_at(30000); assert(brd_measurement_get_display().value == 20000);
    } else if (name == "hold_unlock_generation") {
        baseline(); launch(); pulse_at(24000);
        auto first = brd_measurement_get_display();
        assert(first.hold_active);
        at(50000); brd_measurement_max_frame_presented(first.generation);
        load_at(100000, true);
        update_at(2549999); assert(brd_measurement_get_display().hold_active);
        update_at(2550000); assert(!brd_measurement_get_display().hold_active);
        update_at(2550999); assert(brd_measurement_get_display().show_max);
        update_at(2551000); assert(brd_measurement_get_display().loaded && !brd_measurement_get_display().show_max);
        brd_measurement_max_frame_presented(first.generation);
        assert(!brd_measurement_get_display().hold_active);
    } else if (name == "micros_wrap") {
        uint64_t base = (1ULL << 32) - 10000;
        baseline(base);
        load_at(base + 9500, false);
        pulse_at(base + 12000, false);
        update_at(base + 13000);
        assert(brd_measurement_get_diagnostics().launch_events == 1);
        assert(brd_measurement_get_display().value == 20000);
        pulse_at(base + 27000);
        assert(brd_measurement_get_display().hold_active && brd_measurement_get_display().value == 20000);
    } else if (name == "millis_wrap_hold") {
        uint64_t base = ((1ULL << 32) - 1000) * 1000;
        baseline(base); launch(base); pulse_at(base + 24000);
        auto d = brd_measurement_get_display();
        brd_measurement_max_frame_presented(d.generation);
        update_at(base + 2523999); assert(brd_measurement_get_display().hold_active);
        update_at(base + 2524000); assert(!brd_measurement_get_display().hold_active);
    } else if (name == "no_rpm_timeout") {
        loaded(); pulse_at(3000); load_at(4000, false); update_at(5000);
        update_at(1203999); assert(g_state == BRD_STATE_SPINNING_LAUNCHED);
        update_at(1204000); assert(g_state == BRD_STATE_WAIT_LOAD && !brd_measurement_get_display().show_max);
    } else if (name == "normal_30k") {
        loaded();
        for (uint64_t t = 2000; t <= 300000; t += 2000) pulse_at(t);
        assert(brd_measurement_get_display().value == 30000);
        assert(brd_measurement_get_diagnostics().rpm_queue_overflows == 0);
        brd_measurement_begin();
        assert(brd_measurement_get_display().value == 30000);
    } else if (name == "normal_30k_dual_edge") {
        loaded();
        bool high = false;
        for (uint64_t t = 2000; t <= 300000; t += 1000) {
            rpm_edge_at(t, high);
            high = !high;
        }
        assert(brd_measurement_get_display().value == 30000);
        assert(brd_measurement_get_telemetry().max_rpm == 30000);
        assert(brd_measurement_get_diagnostics().rpm_queue_overflows == 0);
    } else if (name == "adc_single_conversion") {
        brd_battery_begin(); assert(test_adc_reads == 1);
        battery_at(999999, 1850); assert(test_adc_reads == 1);
        battery_at(1000000, 1850); assert(test_adc_reads == 2);
        assert(brd_battery_get_voltage_mv() == 3700 && brd_battery_get_percent() == 20);
        assert(test_adc_units == 1 && test_adc_configs == 1);
    } else if (name == "adc_error_keeps_value") {
        test_adc_mv = 1770; brd_battery_begin();
        battery_at(1000000, 0, ESP_ERR_TIMEOUT);
        assert(brd_battery_get_voltage_mv() == 3540 && brd_battery_get_percent() == 7);
        assert(!brd_battery_is_low_locked() && brd_battery_measurement_allowed());
        auto failed = brd_battery_get_diagnostics();
        assert(failed.status == BRD_BATTERY_READ_ERROR && failed.last_attempt_error == ESP_ERR_TIMEOUT);
        assert(failed.failure_count == 1 && !failed.last_sample_valid);
        battery_at(2000000, 1770);
        auto recovered = brd_battery_get_diagnostics();
        assert(recovered.status == BRD_BATTERY_OK && recovered.last_failure_error == ESP_ERR_TIMEOUT);
        assert(recovered.consecutive_failures == 0 && !brd_battery_is_low_locked());
    } else if (name == "adc_stale") {
        brd_battery_begin();
        battery_at(1000000, 0, ESP_ERR_TIMEOUT); battery_at(2000000, 0, ESP_ERR_TIMEOUT);
        assert(brd_battery_measurement_allowed());
        battery_at(3000000, 0, ESP_ERR_TIMEOUT);
        assert(brd_battery_is_adc_fault() && !brd_battery_measurement_allowed() && !brd_battery_is_low_locked());
        battery_at(4000000, 1770);
        assert(brd_battery_measurement_allowed() && brd_battery_get_percent() == 7);
    } else if (name == "adc_stale_latch_wrap") {
        brd_battery_begin();
        battery_at(3000000, 0, ESP_ERR_TIMEOUT);
        assert(brd_battery_is_adc_fault());
        at((1ULL << 32) * 1000);
        assert(brd_battery_is_adc_fault() && !brd_battery_measurement_allowed());
    } else if (name == "oled_boot_version") {
        setup();
        assert(std::string(PROJECT_VERSION) == "V1.13");
        dump_frame("boot.pgm");
    } else if (name == "deep_sleep_skips_boot") {
        mock_reset_reason = ESP_RST_DEEPSLEEP;
        uint64_t before = test_us;
        setup();
        // [V1.11 測試] Deep-sleep reset 只初始化 OLED，不等待 1.5 秒版本畫面。
        assert(test_us - before < (uint64_t)OLED_BOOT_VERSION_DISPLAY_MS * 1000ULL);
    } else if (name == "adc_boot_error") {
        test_new_error = ESP_ERR_NO_MEM; brd_battery_begin();
        assert(!brd_battery_measurement_allowed() && !brd_battery_is_low_locked());
        assert(brd_battery_get_diagnostics().status == BRD_BATTERY_INIT_ERROR);
        assert(test_adc_reads == 0);
        test_new_error = ESP_OK; battery_at(1000000, 1770);
        assert(brd_battery_measurement_allowed() && test_adc_reads == 1);
    } else if (name == "adc_config_retry") {
        test_config_error = ESP_ERR_INVALID_ARG; brd_battery_begin();
        assert(test_adc_units == 1 && test_adc_reads == 0 && brd_battery_is_adc_fault());
        test_config_error = ESP_OK; battery_at(1000000, 1850);
        assert(test_adc_units == 1 && test_adc_reads == 1 && brd_battery_measurement_allowed());
    } else if (name == "adc_calibration_retry") {
        test_cali_create_error = ESP_ERR_NOT_SUPPORTED; brd_battery_begin();
        assert(brd_battery_get_diagnostics().status == BRD_BATTERY_CALIBRATION_ERROR && test_adc_reads == 0);
        test_cali_create_error = ESP_OK; battery_at(1000000, 1850);
        assert(brd_battery_measurement_allowed() && test_adc_reads == 1 && test_adc_units == 1);
        test_cali_error = ESP_ERR_INVALID_STATE; battery_at(2000000, 0);
        assert(brd_battery_get_voltage_mv() == 3700 && !brd_battery_is_low_locked());
        assert(brd_battery_get_diagnostics().status == BRD_BATTERY_CALIBRATION_ERROR);
    } else if (name == "adc_invalid_data") {
        brd_battery_begin(); test_adc_raw = -1; battery_at(1000000, 1850);
        assert(brd_battery_get_diagnostics().status == BRD_BATTERY_DATA_ERROR);
        test_adc_raw = 2000; battery_at(2000000, -1);
        assert(brd_battery_get_diagnostics().status == BRD_BATTERY_DATA_ERROR);
        assert(brd_battery_get_voltage_mv() == 3700 && !brd_battery_is_low_locked());
    } else if (name == "valid_zero_is_low") {
        test_adc_raw = 0; test_adc_mv = 0; brd_battery_begin();
        assert(brd_battery_is_low_locked() && !brd_battery_is_adc_fault());
        assert(brd_battery_get_diagnostics().status == BRD_BATTERY_OK);
    } else if (name == "exact_five") {
        test_adc_mv = 1750; brd_battery_begin();
        assert(brd_battery_get_percent() == 5 && brd_battery_measurement_allowed());
    } else if (name == "recovery_duration") {
        test_adc_mv = 1730; brd_battery_begin();
        assert(brd_battery_is_low_locked());
        battery_at(1000000, 1800); battery_at(2000000, 1800); battery_at(3000000, 1800);
        assert(!brd_battery_restart_required());
        assert(brd_battery_get_diagnostics().recovery_elapsed_ms == 2000);
        battery_at(4000000, 1800); assert(brd_battery_restart_required());
    } else if (name == "recovery_error_break") {
        test_adc_mv = 1730; brd_battery_begin();
        battery_at(1000000, 1800); battery_at(2000000, 1800);
        battery_at(3000000, 1800, ESP_ERR_TIMEOUT);
        assert(!brd_battery_get_diagnostics().recovery_pending);
        battery_at(4000000, 1800); battery_at(5000000, 1800); battery_at(6000000, 1800);
        assert(!brd_battery_restart_required());
        battery_at(7000000, 1800); assert(brd_battery_restart_required());
    } else if (name == "recovery_low_break") {
        test_adc_mv = 1730; brd_battery_begin();
        battery_at(1000000, 1800); battery_at(2000000, 1800); battery_at(3000000, 1770);
        assert(!brd_battery_get_diagnostics().recovery_pending && brd_battery_is_low_locked());
        battery_at(4000000, 1800); battery_at(5000000, 1800); battery_at(6000000, 1800);
        assert(!brd_battery_restart_required());
        battery_at(7000000, 1800); assert(brd_battery_restart_required());
    } else if (name == "recovery_gap") {
        test_adc_mv = 1730; brd_battery_begin();
        battery_at(1000000, 1800); battery_at(2000000, 1800); battery_at(5000000, 1800);
        assert(!brd_battery_restart_required() && brd_battery_get_diagnostics().recovery_elapsed_ms == 0);
        battery_at(6000000, 1800); battery_at(7000000, 1800); battery_at(8000000, 1800);
        assert(brd_battery_restart_required());
    } else if (name == "recovery_millis_wrap") {
        uint64_t base = ((1ULL << 32) - 2000) * 1000;
        at(base); test_adc_mv = 1730; brd_battery_begin();
        for (uint64_t i = 1; i <= 3; i++) battery_at(base + i * 1000000, 1800);
        assert(!brd_battery_restart_required());
        battery_at(base + 4000000, 1800); assert(brd_battery_restart_required());
    } else if (name == "reboot_policy_unchanged") {
        test_adc_mv = 1730; brd_battery_begin(); battery_at(1000000, 1770);
        assert(brd_battery_is_low_locked()); brd_battery_begin();
        assert(!brd_battery_is_low_locked() && brd_battery_measurement_allowed());
    } else if (name == "main_adc_fault_resume") {
        GPIO.in.val |= 1U << LOAD_IR_GPIO;
        setup(); uint64_t base = test_us;
        for (uint64_t i = 1; i <= 3; i++) {
            at(base + i * 1000000); test_read_error = ESP_ERR_TIMEOUT; loop();
        }
        assert(!g_measurement_enabled && g_frame_is_adc_fault && !brd_battery_is_low_locked());
        at(base + 4000000); test_read_error = ESP_OK; test_adc_mv = 1770; loop();
        assert(g_measurement_enabled && !g_frame_is_adc_fault && !brd_battery_is_low_locked());
        assert(test_restarts == 0);
    } else if (name == "main_low_restart") {
        test_adc_mv = 1730; setup(); assert(!g_measurement_enabled && g_frame_is_low_battery);
        for (uint64_t i = 1; i <= 3; i++) { at(i * 1000000); test_adc_mv = 1800; loop(); }
        assert(test_restarts == 0);
        at(4000000);
        try { loop(); assert(false); } catch (const std::runtime_error &) {}
        assert(test_restarts == 1 && !g_measurement_enabled);
    } else if (name == "oled_adc_boot_recover") {
        test_read_error = ESP_ERR_TIMEOUT; setup();
        assert(g_frame_is_adc_fault && !g_measurement_enabled && test_us == 0);
        dump_frame("adc_error.pgm");
        assert(oled_get_glyph('C') != oled_get_glyph(' '));
        at(1000000); test_read_error = ESP_OK; test_adc_mv = 1770; loop();
        assert(!g_frame_is_adc_fault && g_measurement_enabled);
    } else if (name == "oled_retry_hold") {
        brd_battery_begin(); brd_oled_begin(true); uint64_t base = test_us;
        baseline(base); launch(base); pulse_at(base + 24000);
        uint32_t generation = brd_measurement_get_display().generation;
        test_i2c_error = ESP_ERR_TIMEOUT; brd_oled_update();
        assert(!g_oled_available && !g_max_frame_seen);
        test_i2c_error = ESP_OK; at(base + 1024000); brd_oled_update();
        for (unsigned i = 0; i < 36; i++) { test_us += 1000; brd_oled_update(); }
        assert(g_max_frame_seen && brd_measurement_get_display().generation == generation);
        uint32_t lock_start = g_max_lock_start_ms;
        for (unsigned i = 0; i < 40; i++) { test_us += 1000; brd_oled_update(); }
        assert(g_max_lock_start_ms == lock_start);
        dump_frame("hold.pgm");
    } else if (name == "gpio_pulls") {
        brd_io_begin(); brd_battery_begin(); brd_oled_begin(true);
        assert(test_pull_modes[0] == GPIO_FLOATING && test_pull_modes[1] == GPIO_FLOATING);
        assert(test_pull_modes[3] == GPIO_FLOATING && test_pull_modes[20] == GPIO_FLOATING);
        assert(test_pull_modes[21] == GPIO_FLOATING && test_pull_modes[10] == GPIO_PULLUP_ONLY);
    } else {
        return 2;
    }
    std::cout << "PASS " << name << '\n';
    return 0;
}
