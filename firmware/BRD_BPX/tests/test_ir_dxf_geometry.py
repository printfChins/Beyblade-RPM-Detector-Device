#!/usr/bin/env python3
"""[新增] 確認 OLED IR TEST 六頂點直接來自 DXF 等比例左轉 90 度。"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
dxf = root / 'docs' / 'IR_shape_reference.dxf'
source = (root / 'brd_oled.cpp').read_text(encoding='utf-8')

# DXF ASCII 中每兩列是一組 group code / group value；不需要額外 CAD 套件。
lines = dxf.read_text(encoding='utf-8').splitlines()
assert len(lines) % 2 == 0
pairs = [(lines[i].strip(), lines[i + 1].strip()) for i in range(0, len(lines), 2)]
polygons = []
index = 0
while index < len(pairs):
    code, value = pairs[index]
    if code != '0' or value != 'LWPOLYLINE':
        index += 1
        continue
    index += 1
    poly = []
    last_x = None
    while index < len(pairs) and pairs[index][0] != '0':
        group, raw = pairs[index]
        if group == '10':
            last_x = float(raw)
        elif group == '20' and last_x is not None:
            poly.append((last_x, float(raw)))
            last_x = None
        index += 1
    assert len(poly) == 6, 'DXF 每個 IR 孔洞應為六頂點'
    polygons.append(poly)
assert len(polygons) == 2

# 第一組較寬，是右側梯形；第二組是左側六邊形。
left_dxf, right_dxf = polygons[1], polygons[0]
scale = 3.5

def rotate_left(poly):
    xmax = max(x for x, y in poly)
    ymax = max(y for x, y in poly)
    return {(round((ymax-y) * scale), round((xmax-x) * scale)) for x, y in poly}

def actual_vertices(name):
    m = re.search(r'static const int8_t ' + name + r'\[6\]\[2\]\s*=\s*\{([^;]+)\};', source, re.S)
    assert m, f'找不到圖形頂點：{name}'
    return {(int(x), int(y)) for x, y in re.findall(r'\{\s*(-?\d+)\s*,\s*(-?\d+)\s*\}', m.group(1))}

assert actual_vertices('hexagon') == rotate_left(left_dxf), 'IR2 頂點與 DXF 左轉 90 度不一致'
assert actual_vertices('trapezoid') == rotate_left(right_dxf), 'IR1 頂點與 DXF 左轉 90 度不一致'

# 右側旋轉後上下對稱、但非左右對稱；不可恢復錯誤鏡射。
trapezoid = actual_vertices('trapezoid')
min_y, max_y = min(y for x,y in trapezoid), max(y for x,y in trapezoid)
min_x, max_x = min(x for x,y in trapezoid), max(x for x,y in trapezoid)
assert {(x,min_y+max_y-y) for x,y in trapezoid} == trapezoid
assert {(min_x+max_x-x,y) for x,y in trapezoid} != trapezoid
assert re.search(r'69U,\s*8U,\s*trapezoid,\s*6U,\s*ir1_triggered,\s*IR_MIRROR_Y', source), 'IR1 應僅對上下鏡射'
assert re.search(r'42U,\s*12U,\s*hexagon,\s*6U,\s*ir2_triggered', source), 'IR2 位置應為 x=42'
# [新增] 只移動原點，不能改變 DXF 圖形座標或兩軸對稱判定。
assert (69 - (42 + 16) - 1) == 10, '兩圖應維持 10px 空白間距'
print('PASS: both OLED polygons match DXF left-90-degree vertices and correct IR1 symmetry.')
