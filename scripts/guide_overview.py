#!/usr/bin/env python3
"""Marks the parts of the window on the overview screenshot of the user guide (docs/guide/overview.png).

Usage: python3 scripts/guide_overview.py screenshot.png docs/guide/overview.png
The screenshot is the whole window at 96 dpi (1240 x 800), a document open with the Pages panel showing.
The numbers match the table in section 2 of the guide (scripts/make_user_guide.py).
"""
import sys
from PIL import Image, ImageDraw, ImageFont

RED = (217, 48, 48)
# (number, box) in window pixels; the number sits on the box's top-left corner (the status bar's in its middle,
# where it covers no text)
PARTS = [
    (1, (40, 4, 240, 40)),       # tabs and +
    (2, (1100, 2, 1238, 40)),    # window buttons
    (3, (10, 46, 172, 82)),      # file buttons
    (4, (186, 46, 748, 82)),     # tool menus
    (5, (808, 46, 974, 82)),     # page navigation
    (6, (986, 46, 1136, 82)),    # zoom
    (7, (1146, 46, 1228, 82)),   # find and more
    (8, (6, 90, 234, 768)),      # side panel
    (9, (250, 102, 1206, 768)),  # page area
    (10, (6, 772, 1236, 796)),   # status bar
]


def main(src, dst):
    im = Image.open(src).convert("RGB").crop((0, 0, 1240, 800))
    d = ImageDraw.Draw(im)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", 15)
    except OSError:
        font = ImageFont.load_default()
    for n, (l, t, r, b) in PARTS:
        d.rounded_rectangle((l, t, r, b), radius=6, outline=RED, width=2)
    for n, (l, t, r, b) in PARTS:
        cx, cy, rad = (l + 4, t + 4, 11) if n != 10 else ((l + r) // 2, t + 2, 11)
        cx, cy = max(cx, rad + 1), max(cy, rad + 1)
        d.ellipse((cx - rad, cy - rad, cx + rad, cy + rad), fill=RED, outline=(255, 255, 255), width=2)
        d.text((cx, cy), str(n), fill=(255, 255, 255), font=font, anchor="mm")
    im.save(dst)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
