"""[V0.12 新增] 執行原始模組的主機邏輯回歸；不代表 ESP32-C3 實機驗證。"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

CASES = """rpm_dual_edge_change_trigger rpm_dual_edge_full_revolution rpm_dual_edge_half_rev_update rpm_dual_edge_micros_wrap launch_uses_latest_dual_edge_rpm threshold35 one_dropped_edge spike_policy_unchanged load_backlog
load_short_bounce load_exact_debounce load_rpm_chronology timeout_chronology
unload_before_first_rpm load_overflow rpm_overflow hold_unlock_generation
micros_wrap millis_wrap_hold no_rpm_timeout normal_30k normal_30k_dual_edge adc_single_conversion
adc_error_keeps_value adc_stale adc_stale_latch_wrap oled_boot_version deep_sleep_skips_boot adc_boot_error adc_config_retry
adc_calibration_retry adc_invalid_data valid_zero_is_low exact_five
recovery_duration recovery_error_break recovery_low_break recovery_gap
recovery_millis_wrap reboot_policy_unchanged main_adc_fault_resume main_low_restart
 oled_adc_boot_recover oled_retry_hold gpio_pulls""".split()

BLE_CASES = """state_event_indication state_indication_timeout state_confirmation_timeout state_millis_wrap reliable_only_protocol reliable_transfer reliable_send_timeout reliable_status_timeout
reliable_retry_ack reliable_restart_abort reliable_new_load_cancels_ack reliable_reconnect_restart
timeout_result_blocks_sleep power_idle_sleep power_sleep_shutdown_failure power_idle_abort
oled_connection_icon oled_icon_hold oled_icon_warning offline_record event_curve_backlog
launch_raw_snapshot curve_cap_and_wrap curve_micros_wrap gatt_identity
live_cadence_and_change notify_failure_retries send_timeout command_queue_bounds
init_failure_retry shutdown_failure_retry power_fault_shutdown_resume low_battery_no_ble ble_millis_wrap""".split()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    compiler = os.environ.get('CXX', 'g++')
    for source in sorted((root.parent / 'BRD_BLE_OLED').iterdir()):
        if source.suffix in ('.cpp', '.ino'):
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wshadow',
                            '-pedantic', '-fsyntax-only', '-x', 'c++', '-I', str(root / 'host'),
                            str(source)], check=True)
            print(f'PASS standalone syntax: {source.name}', flush=True)
    with tempfile.TemporaryDirectory(prefix='brd_v012_') as work:
        destination = args.output.resolve() if args.output else Path(work)
        destination.mkdir(parents=True, exist_ok=True)
        binary = Path(work) / 'test_v012'
        command = [compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                   '-Wshadow', '-pedantic', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                   '-I', str(root / 'host'), str(root / 'host/test_v012.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        for case in CASES:
            subprocess.run([str(binary), case], check=True, cwd=destination, env=environment)
        command[-3] = str(root / 'host/test_ble.cpp')
        subprocess.run(command, check=True)
        for case in BLE_CASES:
            subprocess.run([str(binary), case], check=True, cwd=destination, env=environment)
        print(f'{len(CASES) + len(BLE_CASES)} cases passed; hardware interfaces are mocked.')

if __name__ == '__main__':
    main()
