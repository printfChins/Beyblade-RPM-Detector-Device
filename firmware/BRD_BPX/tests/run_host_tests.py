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
    "battery": ["tests/test_battery_curve.cpp", "brd_battery.cpp"],
    "model": ["tests/test_model.cpp", "brd_settings_model.cpp"],
    "runtime": ["tests/test_runtime.cpp", "brd_measurement.cpp", "brd_capture.cpp", "brd_settings.cpp",
                "brd_settings_model.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp"],
    "ble": ["tests/test_ble.cpp", "brd_settings_ble.cpp", "brd_settings_model.cpp"],
    "bbp_link": ["tests/test_bbp_link.cpp", "brd_bbp.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp", "brd_scheduler.cpp"],
    "auto": ["tests/test_auto_baseline.cpp", "brd_measurement.cpp", "brd_capture.cpp"],
    "boot": ["tests/render_boot.cpp"],
    "schedule": ["tests/test_schedule.cpp", "brd_scheduler.cpp"],
    "priority_ble": ["tests/test_priority_ble.cpp", "brd_measurement.cpp", "brd_capture.cpp",
                     "brd_settings.cpp", "brd_settings_model.cpp", "brd_settings_ble.cpp",
                     "brd_bbp.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp",
                     "brd_ota.cpp", "brd_ota_protocol.cpp", "brd_scheduler.cpp"],
    "priority": ["tests/test_priority.cpp", "brd_measurement.cpp", "brd_capture.cpp",
                 "brd_settings.cpp", "brd_settings_model.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp"],
    "ota_protocol": ["tests/test_ota_protocol.cpp", "brd_ota_protocol.cpp"],
    "ota_runtime": ["tests/test_ota_runtime.cpp", "brd_ota.cpp", "brd_ota_protocol.cpp", "brd_settings_model.cpp"],
    "ota_history": ["tests/test_ota_history.cpp", "brd_bbp.cpp", "brd_bbp_session.cpp", "brd_bbp_protocol.cpp"],
}


def run(args):
    subprocess.run(args, cwd=ROOT, env=ENV, check=True)


for name, sources in CASES.items():
    executable = BIN / name
    libraries = ["-lcrypto"] if name in ("ota_runtime", "priority_ble") else []
    # [V1.4 新增] battery-only ESP-IDF ADC stub 不影響其他測試的 GPIO/ADC 模擬。
    battery_flags = ["-Itests/battery_stubs"] if name == "battery" else []
    run(FLAGS + battery_flags + sources + libraries + ["-o", str(executable)])
    arguments = [str(executable)]
    if name == "boot":
        arguments.append(str(BIN / "boot.pgm"))
    if name == "priority":
        for case in range(1,14): run(arguments+[str(case)])
    elif name == "bbp_link":
        # [V1.4 LOAD Notify 修改] 追加 13~14：狀態切換與曲線完整性、BLE 低功耗重試。
        for case in range(1,15): run(arguments+[str(case)])
    else: run(arguments)
    if name == "boot":
        # [V1.2 新增] OTA 專用扇形進度逐階段檢視，其餘驗證/完成畫面維持。
        # [V1.4 OLED 修改] charge0..chargeC 為不同時間點的靜態畫面驗證，必須全數一致。
        for frame in ["ota", "ota0", "ota25", "ota50", "ota75", "ota100", "verify", "done", "irnone", "ir1", "ir2", "irboth", "wait", "loaded", "max", "hold", "charging", "full", "charge0", "charge1", "charge2", "charge3", "charge4", "charge5", "charge6", "charge7", "charge8", "charge9", "chargeA", "chargeB", "chargeC"]:
            run([str(executable), str(BIN / (frame + ".pgm")), frame])

run(FLAGS + ["-fsyntax-only", "brd_bbp.cpp", "brd_io.cpp"])
run(FLAGS + ["-x", "c++", "-fsyntax-only", "BRD_BPX.ino"])
run(["python3", "tests/test_ir_dxf_geometry.py"])
print("PASS: 13 host executables, 14 BBP scenarios, RPM priority, OLED SP single-space, battery icon without percentage text, charging without animation, FULL and BLE checks.")
