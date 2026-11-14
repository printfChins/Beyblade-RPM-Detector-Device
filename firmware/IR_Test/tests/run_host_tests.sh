#!/usr/bin/env bash
# [新增] 無 Arduino-ESP32 工具鏈時的 C++ 協定及橋接靜態測試；非實機驗證。
set -euo pipefail
cd "$(dirname "$0")/.."
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT
FLAGS=(-std=c++17 -Wall -Wextra -Werror -I. -Itests/stubs)
g++ "${FLAGS[@]}" tests/test_ota_protocol.cpp brd_ota_protocol.cpp -o "$TEMP_DIR/protocol"
"$TEMP_DIR/protocol"
g++ "${FLAGS[@]}" tests/test_ir_test_guard.cpp ir_test_ota_guard.cpp -o "$TEMP_DIR/guard"
"$TEMP_DIR/guard"
g++ "${FLAGS[@]}" tests/test_ir_ota_runtime.cpp ir_test_ota.cpp \
    brd_ota_protocol.cpp ir_test_ota_guard.cpp -lcrypto -o "$TEMP_DIR/runtime"
"$TEMP_DIR/runtime"
g++ "${FLAGS[@]}" -fsyntax-only ir_test_ota.cpp
# .ino 需要加入測試 stub 中的 GPIO / Wire / Arduino API
# -x c++ 明確指定 .ino 的語言。
g++ "${FLAGS[@]}" -x c++ -fsyntax-only IR_Test_OTA.ino
