#!/usr/bin/env python3
"""[新增] 模擬 Arduino ESP32 Core 定義 PI 巨集時，兩個 UI 模組仍可編譯。"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
command = [
    "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
    "-Itests/stubs", "-I.",
    "-DPI=3.1415926535897932384626433832795",
    "-fsyntax-only", "brd_ui_warning_pages.cpp", "brd_ui_style_scene.cpp",
]
subprocess.run(command, cwd=root, check=True)
print("PASS: Arduino ESP32 Core PI macro compatibility")
