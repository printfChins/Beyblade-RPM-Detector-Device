#!/usr/bin/env python3
"""Render warning pages with the production scene+Dirty presenter, then export PNG/GIF.
Host-only renderer; this script and the GIFs are not included in the MCU firmware build.
"""
from __future__ import annotations
from pathlib import Path
import argparse
import csv
import subprocess
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
STYLE_NAMES = ["NEBULA", "DIAL", "TORQUE"]
PAGES = [(5, "LOW_BATTERY"), (6, "ADC_FAULT")]

def compile_host(executable: Path) -> None:
    args = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-Itests/stubs", "-I.", "tests/render_exact_style.cpp",
            "brd_ui_motion.cpp", "brd_ui_style_scene.cpp",
            "brd_ui_warning_pages.cpp", "brd_ui_dirty_presenter.cpp",
            "-o", str(executable)]
    subprocess.run(args, cwd=ROOT, check=True)

def from_raw(path: Path) -> Image.Image:
    data = np.frombuffer(path.read_bytes(), dtype="<u2").reshape((80, 160))
    rgb = np.stack((((data >> 11) & 31) * 255 // 31,
                    ((data >> 5) & 63) * 255 // 63,
                    (data & 31) * 255 // 31), axis=2).astype('uint8')
    return Image.fromarray(rgb, 'RGB')

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    raw = out / "raw"
    raw.mkdir(exist_ok=True)
    exe = out / "host_renderer"
    compile_host(exe)
    report=[]
    # 3 themes x 2 pages, each 20 samples at 500ms. The final 12 are static.
    for idx, name in enumerate(STYLE_NAMES):
        for pg, page_name in PAGES:
            capture = subprocess.check_output([str(exe), str(idx), str(pg), str(raw)], text=True).strip()
            style, page, payload, regions = [int(v) for v in capture.split(',')]
            assert style == idx and page == pg
            report.append((idx+1, name, page_name, payload, regions))
            first = from_raw(raw / f"t{idx:02d}_p{pg}_f00.rgb565")
            first.save(out / f"{idx+1:02d}_{name}_{page_name}_160x80.png")
            first.resize((640, 320), Image.Resampling.NEAREST).save(out / f"{idx+1:02d}_{name}_{page_name}_4x.png")
    for pg,page_name in PAGES:
        contact = Image.new('RGB', (3*328,1*184), '#091019')
        d = ImageDraw.Draw(contact)
        for idx,name in enumerate(STYLE_NAMES):
            im = Image.open(out / f"{idx+1:02d}_{name}_{page_name}_160x80.png")
            x=(idx%3)*328;y=(idx//3)*184
            contact.paste(im.resize((320,160),Image.Resampling.NEAREST),(x,y+19))
            d.text((x+3,y+3),f"{idx+1:02d} {name}",fill="#dce6ef")
        contact.save(out / f"ALL_{page_name}_3_STYLES.png")
        # Re-render each animation frame exactly from C++ RGB565 framebuffer.
        gif_frames=[]
        for frame in range(8):
            board=Image.new('RGB',(3*328,1*184),'#091019');draw=ImageDraw.Draw(board)
            for idx,name in enumerate(STYLE_NAMES):
                i=from_raw(raw/f"t{idx:02d}_p{pg}_f{frame:02d}.rgb565")
                x=(idx%3)*328;y=(idx//3)*184
                board.paste(i.resize((320,160),Image.Resampling.NEAREST),(x,y+19))
                draw.text((x+3,y+3),f"{idx+1:02d} {name}",fill="#dce6ef")
            gif_frames.append(board)
        gif_frames[0].save(out/f"ALL_{page_name}.gif",save_all=True,
             append_images=gif_frames[1:],duration=500,loop=0,optimize=True)
    with (out/'SPI_TRANSFER_REPORT.csv').open('w', newline='') as f:
        wr=csv.writer(f);wr.writerow(['theme','name','page','transfer_bytes_20frames','transfer_regions_20frames'])
        wr.writerows(report)
    print(f"Exported 6 style PNG + 2 contact sheets + 2 GIF in {out}")

if __name__ == '__main__':
    main()
