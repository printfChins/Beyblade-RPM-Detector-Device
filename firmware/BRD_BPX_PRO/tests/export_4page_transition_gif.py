#!/usr/bin/env python3
"""Produce four-page transition GIFs from real C++ RGB565 transition frames."""
from pathlib import Path
import subprocess
import tempfile
import numpy as np
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
out = root / 'docs' / 'ui_transition_previews'
out.mkdir(parents=True, exist_ok=True)
exe = root/'tests'/'bin'/'render_4page_transitions'
exe.parent.mkdir(parents=True, exist_ok=True)
cmd = ['g++','-std=c++17','-O2','-Itests/stubs','-I.',
       'tests/export_transition_frames.cpp','brd_ui_transition.cpp',
       'brd_ui_style_scene.cpp','brd_ui_warning_pages.cpp',
       'brd_ui_dirty_presenter.cpp','-o',str(exe)]
subprocess.run(cmd, cwd=root, check=True)
styles=['NEBULA','DIAL','TORQUE']
pages=['MAIN','GRAPH','HISTORY','IR TEST']
W,H=160,80

def read_rgb565(p):
    raw=np.fromfile(p,dtype='<u2')
    assert raw.size==W*H, p
    r=((raw>>11)&31).astype(np.uint16)
    g=((raw>>5)&63).astype(np.uint16)
    b=(raw&31).astype(np.uint16)
    rgb=np.stack(((r*255//31),(g*255//63),(b*255//31)),axis=-1).astype(np.uint8)
    return Image.fromarray(rgb.reshape((H,W,3)),'RGB')

def add_label(img, style, label):
    w,h=img.size
    background=Image.new('RGB',(w,h+18),(3,8,18))
    background.paste(img,(0,18))
    ImageDraw.Draw(background).text((5,3),f'{style}  {label}',fill='white',font=ImageFont.load_default())
    return background

with tempfile.TemporaryDirectory() as tmp:
    subprocess.run([str(exe),tmp],cwd=root,check=True)
    frames_by_style=[]
    for i,style in enumerate(styles):
        frames=[]; durations=[]
        for page, label in enumerate(pages):
            for step in range(6):
                rgb=read_rgb565(Path(tmp)/f'style{i}_page{page}_step{step}.rgb565')
                frames.append(add_label(rgb.resize((640,320),Image.Resampling.NEAREST),style,label))
                durations.append(450 if step==0 else 100)
        path=out/f'{i+1:02d}_{style}_4PAGE_TRANSITION.gif'
        frames[0].save(path,save_all=True,append_images=frames[1:],
                       duration=durations,loop=0,optimize=False)
        frames_by_style.append(frames)
        print('saved',path.name)

    # [修改] 3x1 grid; all three designs are updated from the same C++ step.
    allframes=[]
    for fi in range(len(frames_by_style[0])):
        grid=Image.new('RGB',(320*3, 180),(3,8,18))
        for style_index,style_frames in enumerate(frames_by_style):
            # Scale from original 640x320 to 320x160 for a 3x3 comparison.
            src=style_frames[fi].crop((0,18,640,338)).resize((320,160),Image.Resampling.NEAREST)
            labeled=add_label(src, styles[style_index],pages[min(fi//6,3)])
            x=(style_index%3)*320
            y=0
            grid.paste(labeled,(x,y))
        allframes.append(grid)
    durations=[450 if frame%6==0 else 100 for frame in range(len(allframes))]
    allframes[0].save(out/'ALL_3_4PAGE_TRANSITIONS.gif',save_all=True,
        append_images=allframes[1:],duration=durations,loop=0,optimize=False)
    print('saved ALL_3_4PAGE_TRANSITIONS.gif')
