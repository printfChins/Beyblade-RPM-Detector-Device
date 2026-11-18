#!/usr/bin/env python3
"""
[修改] V1.4.5 中位數／兩筆停止測試已不適用，改為 FALLING 原始取樣與 BLE 延後傳輸。
[保留] 原有 BBP CRC / 名稱安全性回歸與 LOAD/AUTO、方向無關的正負電位模擬。
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / 'tests/bin'
BIN.mkdir(exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--build-only', action='store_true')
parser.add_argument('--skip-build', action='store_true')
args = parser.parse_args()
flags = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
         '-fno-omit-frame-pointer', '-Itests/stubs', '-I.']
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
common = ['brd_bbp.cpp', 'brd_ble_identity.cpp', 'brd_bbp_session.cpp', 'brd_bbp_protocol.cpp']
if not args.skip_build:
    for name, sources in {
        'measurement_fixes': ['tests/test_measurement_fixes.cpp', 'brd_measurement.cpp',
                              'brd_settings.cpp', 'brd_settings_model.cpp'],
        'bbp_recovery': ['tests/test_bbp_recovery.cpp'],
    }.items():
        subprocess.run(flags + sources + common + ['-o', str(BIN / name)], cwd=ROOT, check=True)
if args.build_only:
    raise SystemExit(0)
checks = 0
for case in ['history-crc', 'history-length', 'history-version', 'history-read', 'history-open',
             'history-write-fail', 'pending-clear', 'rename-pending', 'rename-success',
             'rename-before-check', 'rename-at-stop', 'rename-after-stop', 'rename-stop-fail',
             'rename-set-fail', 'rename-gap-fail', 'rename-start-fail']:
    subprocess.run([str(BIN / 'bbp_recovery'), case], cwd=ROOT, env=env, check=True)
    checks += 1
for mode in ['load', 'auto']:
    for polarity in ['positive', 'negative']:
        for case in ['ble-deferred', 'clear-ready', 'clear-spinning', 'clear-hold',
                     'high-after-32', 'startup-three']:
            subprocess.run([str(BIN / 'measurement_fixes'), case, mode, polarity],
                           cwd=ROOT, env=env, check=True)
            checks += 1
print(f'PASS: {checks} FALLING capture / deferred BLE / CRC / rename regression scenarios (ASan/UBSan).')
