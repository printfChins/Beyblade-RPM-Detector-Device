#!/usr/bin/env python3
"""[新增] 編譯後檢查 IR_Test_OTA 的 app-only BIN 與 BRD Web OTA 的相容性。"""
import hashlib
import sys
from pathlib import Path

APP_CAPACITY = 0x140000
REFERENCE_TAG = b"BRD_BPX_OTA_IMAGE_V1_C3"
IR_TEST_TAG = b"BRD_IR_TEST_OTA_IMAGE_V1_C3"


def check(path: Path) -> int:
    if not path.is_file():
        print(f"FAIL: 檔案不存在：{path}")
        return 1
    data = path.read_bytes()
    errors = []
    if not (36 <= len(data) <= APP_CAPACITY):
        errors.append(f"大小須在 36 ~ {APP_CAPACITY} bytes 範圍")
    if len(data) >= 36:
        if data[0] != 0xE9:
            errors.append("映像第一個 byte 不是 ESP image magic 0xE9")
        if int.from_bytes(data[12:14], "little") != 5:
            errors.append("晶片 ID 不是 ESP32-C3 (5)")
        if int.from_bytes(data[32:36], "little") != 0xABCD5432:
            errors.append("不是純 app BIN（可能是 merged / bootloader）")
    if REFERENCE_TAG not in data:
        errors.append("缺少 BRD_BPX OTA V1.2 相容標記")
    if IR_TEST_TAG not in data:
        errors.append("缺少 IR_Test 專屬標記，可能燒錯專案")
    if errors:
        print(f"FAIL: {path.name}")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"PASS: {path.name}")
    print(f"  APP image size: {len(data)} / {APP_CAPACITY} bytes")
    print(f"  MD5: {hashlib.md5(data).hexdigest()}")
    print("  兩組 OTA 標記、映像 header 與容量檢查通過")
    print("  備註：這不是 Espressif 影像簽章驗證或實機測試")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("用法：python tools/verify_ota_bin.py IR_Test_OTA.ino.bin")
        sys.exit(2)
    sys.exit(check(Path(sys.argv[1])))
