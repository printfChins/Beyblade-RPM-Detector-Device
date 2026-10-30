#!/usr/bin/env python3
"""[新增] Build and run real firmware modules against host hardware boundaries."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "tests" / "bin"
BIN.mkdir(exist_ok=True)
FLAGS = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
         "-Itests/stubs", "-I."]
ENV = os.environ.copy()
# LeakSanitizer cannot inspect /proc under some execution tracers.
# AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.
ENV["ASAN_OPTIONS"] = ENV.get("ASAN_OPTIONS", "") + ":detect_leaks=0"
ENV["UBSAN_OPTIONS"] = ENV.get("UBSAN_OPTIONS", "") + ":halt_on_error=1"
CASES = {
    "model": ["tests/test_model.cpp", "brd_settings_model.cpp"],
    "runtime": ["tests/test_runtime.cpp", "brd_measurement.cpp", "brd_settings.cpp",
                "brd_settings_model.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp"],
    "ble": ["tests/test_ble.cpp", "brd_settings_ble.cpp", "brd_settings_model.cpp"],
    "auto": ["tests/test_auto_baseline.cpp", "brd_measurement.cpp"],
    "boot": ["tests/render_boot.cpp"],
    "ota_protocol": ["tests/test_ota_protocol.cpp", "brd_ota_protocol.cpp"],
    "ota_runtime": ["tests/test_ota_runtime.cpp", "brd_ota.cpp", "brd_ota_protocol.cpp", "brd_settings_model.cpp"],
    # [V1.2.1 新增] V1.2 修正後的 BLE 斷線回歸測試；電池 stub 另行編譯。
    "ota_history": ["tests/test_ota_history.cpp", "brd_bbp.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp"],
    # [V1.2.2 新增] 真實量測與 Session：確認 HOLD 不受 LOAD 彈跳中斷，且需真實解除後再裝載。
    "hold_guard": ["tests/test_hold_guard.cpp", "brd_measurement.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp"],
    # [V1.2.3 新增] 依使用者五步驟驗證 LOAD、2.5s HOLD、MAX、BLE 歷史及重新裝載。
    "load_hold_v123": ["tests/test_load_hold_v123.cpp", "brd_measurement.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp"],
}


def run(args):
    subprocess.run(args, cwd=ROOT, env=ENV, check=True)


for name, sources in CASES.items():
    executable = BIN / name
    libraries = ["-lcrypto"] if name == "ota_runtime" else []
    run(FLAGS + sources + libraries + ["-o", str(executable)])
    arguments = [str(executable)]
    if name == "boot":
        arguments.append(str(BIN / "boot.pgm"))
    run(arguments)
    if name == "boot":
        # [V1.2 新增] OTA 專用扇形進度逐階段檢視，其餘驗證/完成畫面維持。
        for frame in ["ota", "ota0", "ota25", "ota50", "ota75", "ota100", "verify", "done", "irnone", "ir1", "ir2", "irboth"]:
            run([str(executable), str(BIN / (frame + ".pgm")), frame])

run(FLAGS + ["-fsyntax-only", "brd_bbp.cpp", "brd_io.cpp"])
run(FLAGS + ["-x", "c++", "-fsyntax-only", "BRD_BPX.ino"])
run(["python3", "tests/test_ir_dxf_geometry.py"])
# [V1.2.1 修改] 統一於附加測試結束後才輸出總結。

# [V1.2.1 新增] 預防 V1.2 舊版因佇列或 Notify 逾時主動斷線。
run(FLAGS + ["tests/test_bbp_link.cpp", "brd_bbp.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp", "-o", str(BIN / "bbp_link")])
for case in range(1, 7):
    run([str(BIN / "bbp_link"), str(case)])
# [V1.2.1 新增] 不修改 RPM ISR 或充放電取樣頻率的電池模型驗證。
run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
     "-Itests/battery_stubs", "-Itests/stubs", "-I.",
     "tests/test_battery_curve.cpp", "brd_battery.cpp", "-o", str(BIN / "battery_curve")])
run([str(BIN / "battery_curve")])
print("PASS: V1.4.2 LOAD state machine / 2.5s HOLD / BLE/battery / OTA host regression tests.")
