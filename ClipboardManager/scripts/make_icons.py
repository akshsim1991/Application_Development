#!/usr/bin/env python3
"""Draws the Clipboard Manager icons (normal and paused) and writes multi-size .ico files. Needs Pillow."""
import sys, os
from PIL import Image, ImageDraw

def draw(size, paused):
    s = size * 4                       # draw large, then shrink for smooth edges
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    board = (128, 128, 128) if paused else (0, 103, 192)
    dark = (90, 90, 90) if paused else (0, 70, 140)
    r = s // 7
    d.rounded_rectangle((s * 0.14, s * 0.12, s * 0.86, s * 0.94), r, fill=board)
    d.rounded_rectangle((s * 0.22, s * 0.24, s * 0.78, s * 0.86), r // 2, fill=(255, 255, 255))
    d.rounded_rectangle((s * 0.33, s * 0.05, s * 0.67, s * 0.24), r // 2, fill=dark)
    d.rounded_rectangle((s * 0.40, s * 0.09, s * 0.60, s * 0.17), r // 4, fill=(255, 255, 255))
    for i, w in enumerate((0.46, 0.46, 0.34, 0.40)):
        y = s * (0.36 + i * 0.12)
        d.rounded_rectangle((s * 0.30, y, s * (0.30 + w), y + s * 0.045), s // 60, fill=board)
    if paused:
        d.rounded_rectangle((s * 0.58, s * 0.62, s * 0.66, s * 0.84), s // 80, fill=(220, 110, 40))
        d.rounded_rectangle((s * 0.70, s * 0.62, s * 0.78, s * 0.84), s // 80, fill=(220, 110, 40))
    return im.resize((size, size), Image.LANCZOS)

out = sys.argv[1] if len(sys.argv) > 1 else "src/res"
os.makedirs(out, exist_ok=True)
for name, paused in (("clipboard.ico", False), ("clipboard_paused.ico", True)):
    big = draw(256, paused)
    big.save(os.path.join(out, name), sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (256, 256)])
print("icons written to", out)
