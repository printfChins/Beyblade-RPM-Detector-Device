#!/usr/bin/env python3
"""[新增][主機端] 由正式 brd_ui_style_scene.cpp 產生 20 主題低電量 GIF。
僅為預覽工具，編譯結果不進入 ESP32-C3 韌體；需要 g++/numpy/Pillow。
"""
from pathlib import Path
import subprocess
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
DEST = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / 'docs' / 'low_battery_20_styles'
DEST.mkdir(parents=True, exist_ok=True)
raw = DEST / 'raw'
raw.mkdir(exist_ok=True)
exe = DEST / 'render_low_battery_host'
subprocess.run([
    'g++', '-std=c++17', '-O2', '-Itests/stubs', '-I.',
    'tests/render_exact_style.cpp', 'brd_ui_motion.cpp',
    'brd_ui_style_scene.cpp', 'brd_ui_warning_pages.cpp', 'brd_ui_dirty_presenter.cpp', '-o', str(exe)
], cwd=ROOT, check=True)

names = ['NEBULA','DIAL','TORQUE']

def decode(path):
    data = np.fromfile(path, dtype='<u2').reshape((80, 160))
    r = ((data >> 11) & 31) * 255 // 31
    g = ((data >> 5) & 63) * 255 // 63
    b = (data & 31) * 255 // 31
    rgb = np.stack((r,g,b),axis=2).astype(np.uint8)
    return Image.fromarray(rgb,'RGB')

for style in range(3):
    subprocess.run([str(exe), str(style), '5', str(raw)], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)

COLS = 3
CELL_W, CELL_H = 320, 182
labels = ImageFont.load_default()
frames = []
for frame in range(20):
    board = Image.new('RGB',(COLS*CELL_W,1*CELL_H),(9,13,21))
    draw = ImageDraw.Draw(board)
    for style in range(3):
        x = (style%3)*CELL_W
        y = (style//3)*CELL_H
        draw.text((x+6,y+3),f'{style+1:02d} {names[style]}',fill=(229,237,244),font=labels)
        panel = decode(raw/f't{style:02d}_p5_f{frame:02d}.rgb565').resize((320,160),Image.Resampling.NEAREST)
        board.paste(panel,(x,y+22))
    frames.append(board)

# 長時間停留在最後警告頁面，以反映 MCU 只播放 8 幀的節電模式。
durations = [500]*8+[500]*11+[1800]
frames[0].save(DEST/'ALL_LOW_BATTERY.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=True)
frames[0].save(DEST/'ALL_LOW_BATTERY_3_STYLES.png')
frames[-1].save(DEST/'ALL_LOW_BATTERY_FINAL.png')
for style in range(3):
    panel = decode(raw/f't{style:02d}_p5_f00.rgb565')
    panel.save(DEST/f'{style+1:02d}_{names[style]}_160x80.png')
    panel.resize((640,320),Image.Resampling.NEAREST).save(DEST/f'{style+1:02d}_{names[style]}_4x.png')

assert len(frames)==20
print('Generated 3 style preview PNGs and ALL_LOW_BATTERY.gif')
