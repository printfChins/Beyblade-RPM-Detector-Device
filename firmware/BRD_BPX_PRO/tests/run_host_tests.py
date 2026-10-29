#!/usr/bin/env python3
"""[修改] 2026-10-29：與最新 TORQUE-only 測試共用唯一回歸入口。

舊版 run_host_tests.py 依賴已淘汰的 GFX 畫面座標與連結來源，
改為呼叫 run_motion_tests.py，包含 GPIO/RPM/ADC/BLE/OTA、TORQUE 與 25 FPS。
"""
import subprocess
import sys
from pathlib import Path

if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    subprocess.run([sys.executable, str(root / "tests" / "run_motion_tests.py")],
                   cwd=root, check=True)
