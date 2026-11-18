#!/usr/bin/env python3
"""[V1.4.5 新增] 四項缺陷與必要邊界的真實模組回歸。"""
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
        'measurement_fixes': ['tests/test_measurement_fixes.cpp', 'brd_measurement.cpp', 'brd_settings.cpp', 'brd_settings_model.cpp'],
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
# [本次修改] 僅驗證固定 Falling -> Falling，移除已廢棄的 rising 量測案例。
for mode in ['load', 'auto']:
    for polarity in ['falling']:
        for case in ['rising-only', 'clear-ready', 'clear-spinning', 'clear-hold', 'single-low', 'high-after-32',
                     'wrap-zero', 'startup-short', 'startup-three', 'overflow-single-low']:
            subprocess.run([str(BIN / 'measurement_fixes'), case, mode, polarity], cwd=ROOT, env=env, check=True)
            checks += 1
        for phase in [100, 500, 900, 1000, 1100, 2000, 2500, 3000, 3900, 4500, 4900]:
            for case in ['noise', 'startup-noise']:
                subprocess.run([str(BIN / 'measurement_fixes'), case, mode, polarity, str(phase)],
                               cwd=ROOT, env=env, check=True)
                checks += 1
print(f'PASS: {checks} four-fix regression scenarios (ASan/UBSan).')
