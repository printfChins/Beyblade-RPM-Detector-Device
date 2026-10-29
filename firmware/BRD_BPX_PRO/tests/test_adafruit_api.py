#!/usr/bin/env python3
"""[修改] 靜態檢查僅由官方 Adafruit ST7735/GFX 執行所有繪圖與傳輸。"""
from pathlib import Path
import re
r = Path(__file__).resolve().parents[1]
files = ['brd_ui_screen.cpp', 'brd_ui_widgets.cpp', 'brd_ui_page_main.cpp',
         'brd_ui_page_ir.cpp', 'brd_ui_page_ota.cpp', 'brd_ui_page_alert.cpp',
         'brd_ui_core.cpp', 'brd_tft.cpp', 'brd_ui_page_graph.cpp', 'brd_ui_curve.cpp', 'brd_ui_input.cpp']
sources = {n: (r/n).read_text(encoding='utf-8') for n in files}
assert '#include <Adafruit_ST7735.h>' in sources['brd_ui_screen.cpp']
assert 'display.initR(TFT_ADAFRUIT_INIT_PROFILE)' in sources['brd_ui_screen.cpp']
assert 'display.setRotation(TFT_ROTATION)' in sources['brd_ui_screen.cpp']
assert 'label_opaque(' in sources['brd_ui_page_main.cpp']
assert 'requested != active_page' in sources['brd_ui_core.cpp']  # [修正] 新版路由名稱
assert sources['brd_ui_core.cpp'].count('lcd().fillScreen(') == 1
for n, src in sources.items():
    stripped = re.sub(r'/\*.*?\*/|//[^\n]*', '', src, flags=re.S)
    for pattern in [r'\bwritePixels\s*\(',r'\bsetAddrWindow\s*\(',
                    r'\bspiWrite\s*\(',
                    r'\bwriteBytes\s*\(',r'\bg_frame\b']:
        assert not re.search(pattern, stripped), f'Custom LCD code in {n}: {pattern}'
    # [新增] Allow official ST77xx sendCommand only for panel color-order MADCTL, never for scene SPI.
    if n != 'brd_ui_screen.cpp':
        assert not re.search(r'\bsendCommand\s*\(', stripped), f'Custom LCD command in {n}'
    if n != 'brd_ui_core.cpp':
        assert 'fillScreen(' not in stripped, f'Unnecessary full clear in {n}'
assert 'brd_tft_begin' in sources['brd_tft.cpp']
assert 'brd_tft_update' in sources['brd_tft.cpp']
assert 'Adafruit_ST7735' not in sources['brd_tft.cpp']
print('PASS: modular Dirty-region framework, Adafruit-only drawing, no full screen clear in widgets')

assert 'display.sendCommand(ST77XX_MADCTL, &madctl, 1U)' in sources['brd_ui_screen.cpp']
