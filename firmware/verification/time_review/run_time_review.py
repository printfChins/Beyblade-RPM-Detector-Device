"""[V1.13 修改] 驗證目前韌體的時間、Timeout 與 rollover 邊界；需要 Python 3 與 g++。"""
from pathlib import Path
import argparse
import hashlib
import os
import subprocess
import tempfile

MEASUREMENT_CASES = [
    'micros_wrap',
    'rpm_dual_edge_micros_wrap',
    'millis_wrap_hold',
    'recovery_millis_wrap',
    'adc_stale_latch_wrap',
    'timeout_chronology',
    'load_exact_debounce',
]

BLE_CASES = [
    'curve_cap_and_wrap',
    'curve_micros_wrap',
    'ble_millis_wrap',
    'reliable_retry_ack',
    'reliable_reconnect_restart',
    'state_millis_wrap',
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--project-root', type=Path, required=True,
                        help='解壓縮根目錄，內含 BRD_BLE_OLED 與 verification')
    args = parser.parse_args()
    root = args.project_root.resolve()
    host = root / 'verification/host'
    assert (root / 'BRD_BLE_OLED/brd_config.h').is_file()

    firmware_files = [p for p in (root / 'BRD_BLE_OLED').iterdir() if p.is_file()]
    fingerprint = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in firmware_files}
    compiler = os.environ.get('CXX', 'g++')
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')

    with tempfile.TemporaryDirectory(prefix='brd_rollover_') as work:
        work = Path(work)
        groups = [
            (host / 'test_v012.cpp', MEASUREMENT_CASES),
            (host / 'test_ble.cpp', BLE_CASES),
        ]
        for index, (source, cases) in enumerate(groups):
            binary = work / f'time_review_{index}'
            subprocess.run([
                compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wshadow',
                '-pedantic', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-I', str(host), str(source), '-o', str(binary)
            ], check=True)
            for case in cases:
                subprocess.run([str(binary), case], check=True, cwd=work, env=environment)

    assert fingerprint == {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in firmware_files}
    print(f'Complete: {len(MEASUREMENT_CASES) + len(BLE_CASES)} V1.13 time/rollover checks passed; firmware unchanged.')


if __name__ == '__main__':
    main()
