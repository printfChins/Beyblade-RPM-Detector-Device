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
}


def run(args):
    subprocess.run(args, cwd=ROOT, env=ENV, check=True)


for name, sources in CASES.items():
    executable = BIN / name
    run(FLAGS + sources + ["-o", str(executable)])
    arguments = [str(executable)]
    if name == "boot":
        arguments.append(str(BIN / "boot.pgm"))
    run(arguments)

run(FLAGS + ["-fsyntax-only", "brd_bbp.cpp", "brd_io.cpp"])
run(FLAGS + ["-x", "c++", "-fsyntax-only", "BRD_BPX.ino"])
print("PASS: five host executables, BBP/I-O and Arduino main syntax checks.")
