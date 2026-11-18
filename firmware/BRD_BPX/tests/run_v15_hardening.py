#!/usr/bin/env python3
"""[V1.5 新增] 可重現的修正驗證；已知無法軟體辨識的固定等距假脈衝列為 XFAIL。"""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
bin_dir = root / 'tests/bin'
bin_dir.mkdir(exist_ok=True)
target = bin_dir / 'v15_hardening'
flags = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
         '-Itests/stubs', '-I.']
sources = ['tests/test_v15_hardening.cpp', 'brd_measurement.cpp',
           'brd_settings.cpp', 'brd_settings_model.cpp', 'brd_bbp.cpp',
           'brd_ble_identity.cpp', 'brd_bbp_session.cpp', 'brd_bbp_protocol.cpp']
env = os.environ.copy()
env['ASAN_OPTIONS'] = 'detect_leaks=0'
env['UBSAN_OPTIONS'] = 'halt_on_error=1'

subprocess.run(flags + sources + ['-o', str(target)], cwd=root, env=env, check=True)
passed = []
failed = []
for mode in ['load', 'auto']:
    for case in ['two-noise-peak', 'startup-repeated-noise', 'startup-three-extra', 'three-noise-peak',
                 'real-acceleration', 'brief-real-peak', 'single-low', 'high-after-32']:
        result = subprocess.run([str(target), case, mode, 'falling'], cwd=root,
                                env=env, capture_output=True, text=True)
        if result.returncode == 0:
            passed.append(f'{mode}/{case}')
        else:
            failed.append(f'{mode}/{case}: {(result.stderr or result.stdout)[-900:]}')
for case in ['loaded-save-deferred', 'loaded-mode-deferred']:
    result = subprocess.run([str(target), case, 'load', 'falling'], cwd=root,
                            env=env, capture_output=True, text=True)
    if result.returncode == 0:
        passed.append('load/'+case)
    else:
        failed.append('load/'+case+': '+(result.stderr or result.stdout)[-900:])
# 此案例有意確認不可從單路均勻下降緣區別「兩倍真實轉速」與假脈衝。
known = subprocess.run([str(target), 'even-spaced-glitches-limit', 'load', 'falling'],
                       cwd=root, env=env, capture_output=True, text=True)
print(f'V1.5 硬化回歸：PASS={len(passed)} FAIL={len(failed)}')
for case in passed: print('PASS:',case)
for case in failed: print('FAIL:',case)
print('XFAIL 已知限制：' + (known.stdout.strip().split('\n')[0] if known.stdout else 'no output')
      + ' (exit='+str(known.returncode)+')')
if known.returncode == 0 or 'KNOWN_LIMIT: expected=12000, reported=24000' not in known.stdout:
    failed.append('XFAIL classification error: even-spaced-glitches-limit')
flash_known = subprocess.run([str(target), 'auto-flash-capture-limit', 'auto', 'falling'],
                             cwd=root, env=env, capture_output=True, text=True)
print('XFAIL 已知限制：' + (flash_known.stdout.strip().split('\n')[0]
      if flash_known.stdout else 'no output') + ' (exit='+str(flash_known.returncode)+')')
if flash_known.returncode == 0 or 'KNOWN_LIMIT: auto Flash pause missed SP=0' not in flash_known.stdout:
    failed.append('XFAIL classification error: auto-flash-capture-limit')
if failed:
    for case in failed: print('FAIL:', case)
    sys.exit(1)
