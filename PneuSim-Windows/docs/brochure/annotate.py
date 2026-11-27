"""Crops the raw screenshots (no title bar, task bar or status bar, so no version text or private details) and draws numbered markers."""
import sys
from PIL import Image, ImageDraw, ImageFont
SRC, OUT = sys.argv[1], sys.argv[2]
FONT = ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf', 24)
TOP, BOTTOM = 24, 1008
MARKS = {
 '16': [(1,205,41),(2,1215,41),(3,1888,112),(4,1650,41),(5,330,188),(6,760,330),(7,1750,500),(8,200,762),(9,790,791),(10,900,905)],
 '15': [(1,1060,64),(2,1100,112),(3,990,72),(4,800,420),(5,900,930),(6,330,824)],
 '17': [(1,215,370),(2,395,497),(3,320,590),(4,70,585),(5,230,655),(6,470,370),(7,520,255)],
}
for n, marks in MARKS.items():
    im = Image.open(f'{SRC}/{n}.png').convert('RGB')
    d = ImageDraw.Draw(im)
    for k, x, y in marks:
        r = 16
        d.ellipse((x-r-2, y-r-2, x+r+2, y+r+2), fill=(255, 255, 255))
        d.ellipse((x-r, y-r, x+r, y+r), fill=(217, 130, 43))
        w = d.textlength(str(k), font=FONT)
        d.text((x - w/2, y - 14), str(k), font=FONT, fill=(255, 255, 255))
    out = im.crop((0, TOP, im.width, BOTTOM))
    if n == '17':
        out = out.crop((0, 195, 640, 690))  # zoom on the circuit
    out.save(f'{OUT}/shot{n}.png', optimize=True)
