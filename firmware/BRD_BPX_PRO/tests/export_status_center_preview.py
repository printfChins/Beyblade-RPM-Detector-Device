#!/usr/bin/env python3
"""[新增] 從 C++ 正式 scene 輸出的 RGB565 畫面產生三風格狀態 GIF/PNG（主機端）。"""
from pathlib import Path
import subprocess
import sys
import re
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
# [修改] GIF 每幀時間從正式韌體設定讀取，避免預覽與實際排程不一致。
config_text=(ROOT/'brd_config.h').read_text(encoding='utf-8')
match=re.search(r'^#define\s+BRD_UI_MAIN_FRAME_INTERVAL_MS\s+(\d+)U',config_text,re.M)
if not match:
    raise RuntimeError('Missing BRD_UI_MAIN_FRAME_INTERVAL_MS')
frame_duration_ms=int(match.group(1))
OUT=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'docs'/'status_center_previews'
OUT.mkdir(parents=True,exist_ok=True)
RAW=OUT/'rgb565'
RAW.mkdir(exist_ok=True)
EXE=OUT/'renderer'
subprocess.run(['g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Itests/stubs','-I.',
 'tests/render_status_center_preview.cpp','brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp',
 'brd_ui_legible_text.cpp','-o',str(EXE)],cwd=ROOT,check=True)
subprocess.run([str(EXE),str(RAW)],check=True)

fontname='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
font=ImageFont.truetype(fontname,18)
small=ImageFont.truetype(fontname,13)
styles=('0  NEBULA','1  DIAL','2  TORQUE')

def decode(path):
    a=np.fromfile(path,dtype='<u2').reshape((80,160))
    rgb=np.stack([((a>>11)&31)*255//31,((a>>5)&63)*255//63,(a&31)*255//31],axis=2).astype('uint8')
    return Image.fromarray(rgb,'RGB')

def build(i):
    W,H=1066,470
    im=Image.new('RGB',(W,H),'#0b1220')
    d=ImageDraw.Draw(im)
    d.text((20,14),'BRD BPX PRO  /  MAIN STATUS  /  160 x 80',font=font,fill='#e9f0fb')
    d.text((20,51),'WAIT LOAD',font=small,fill='#96c8fc')
    d.text((20,245),'LOADED',font=small,fill='#96c8fc')
    for style in range(3):
        x=18+style*352
        d.text((x,76),styles[style],font=font,fill='#e9edf8')
        for state,y in enumerate((102,284)):
            d.rounded_rectangle((x-4,y-4,x+324,y+164),radius=6,fill='#243146')
            img=decode(RAW/f'f{i:02d}_s{style}_q{state}.rgb565')
            im.paste(img.resize((320,160),Image.Resampling.NEAREST),(x,y))
    return im

frames=[build(i) for i in range(32)]
frames[16].save(OUT/'BRD_BPX_PRO_WAIT_LOADED_置中_三風格.png',optimize=True)
# 單一共同調色盤確保 GIF 幀之間配色一致
pal=frames[0].quantize(colors=190,method=Image.Quantize.MEDIANCUT)
q=[im.quantize(palette=pal,dither=Image.Dither.NONE) for im in frames]
q[0].save(OUT/'BRD_BPX_PRO_WAIT_LOADED_置中_三風格.gif',save_all=True,
  append_images=q[1:],duration=frame_duration_ms,loop=0,optimize=True,disposal=2)
print('PASS',OUT)
