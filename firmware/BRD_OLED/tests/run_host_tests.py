#!/usr/bin/env python3
"""[新增] 編譯與執行所有單機模組；不使用任何通訊或 Flash 替身。"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
         "-Itests/stubs", "-I."]
ENV = os.environ.copy()
ENV["ASAN_OPTIONS"] = "detect_leaks=0"
ENV["UBSAN_OPTIONS"] = "halt_on_error=1"
CASES = {
    "measurement": ["tests/test_measurement.cpp", "brd_measurement.cpp"],
    "oled": ["tests/test_oled.cpp"],
    "sketch": ["tests/test_sketch.cpp", "brd_measurement.cpp",
               "brd_oled.cpp", "brd_battery.cpp", "brd_io.cpp"],
}

with tempfile.TemporaryDirectory(prefix="brd_oled_tests_") as temp:
    for mode, name in [(0, "load"), (1, "auto")]:
        for case, sources in CASES.items():
            binary = Path(temp) / f"{name}_{case}"
            subprocess.run(FLAGS + [f"-DBRD_MEASUREMENT_MODE={mode}"] + sources +
                           ["-o", str(binary)], cwd=ROOT, env=ENV, check=True)
            subprocess.run([str(binary)], cwd=ROOT, env=ENV, check=True)
print("PASS: six host executables, LOAD/AUTO, actual OLED and complete sketch link.")
print("NOTE: host hardware stubs do not replace an ESP32 target build or board test.")
