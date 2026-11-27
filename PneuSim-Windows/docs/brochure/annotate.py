"""Crops light-theme screenshots (no title bar, task bar or status bar: no version text or private details) and draws numbered markers.
Usage: python3 annotate.py <folder with the raw screenshots 7.png 11.png 14.png> <output folder>"""
import sys
from PIL import Image, ImageDraw, ImageFont
SRC, OUT = sys.argv[1], sys.argv[2]
FONT = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 24)
# name -> (source file, crop box, markers (number, x, y) in source coordinates)
JOBS = {
 'shot1': ('14', (0, 24, 1920, 1008), [(1,180,64),(2,1215,41),(3,1280,135),(4,1635,41),(5,330,188),(6,760,330),(7,1750,500),(8,200,762),(9,790,791),(10,700,930)]),
 'shot2': ('7', (965, 340, 1885, 905), [(1,1215,385),(2,1305,470),(3,1305,590),(4,1520,630),(5,1540,800)]),
 'shot3': ('11', (560, 185, 1380, 850), [(1,1075,293),(2,1030,321),(3,805,370),(4,1290,515),(5,1177,807)]),
}
for name, (src, box, marks) in JOBS.items():
    im = Image.open(f'{SRC}/{src}.png').convert('RGB')
    d = ImageDraw.Draw(im)
    for k, x, y in marks:
        r = 16
        d.ellipse((x-r-2, y-r-2, x+r+2, y+r+2), fill=(255, 255, 255))
        d.ellipse((x-r, y-r, x+r, y+r), fill=(217, 130, 43))
        w = d.textlength(str(k), font=FONT)
        d.text((x - w/2, y - 14), str(k), font=FONT, fill=(255, 255, 255))
    im.crop(box).save(f'{OUT}/{name}.png', optimize=True)
