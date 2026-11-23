#!/usr/bin/env python3
# Makes the fonts equations are set in when a document was made with TeX (Computer Modern or Latin Modern), from Latin
# Modern Math (GUST Font License, shared/fonts/GUST-FONT-LICENSE.txt):
#   lmmath-upright.ttf  letters, digits, Greek, signs, arrows and operators, upright
#   lmmath-italic.ttf   a-z, A-Z and the Greek small letters as TeX's mathematical italic ones, at their plain code points
#   lmmath-bold.ttf     a-z, A-Z and 0-9 as the mathematical bold ones, at their plain code points
# The italic and bold letters sit at the plain code points (not U+1D434 ...) so text copied from an equation reads as
# letters, and every system's PDF library can write them. Each font has TrueType outlines (as PDF libraries embed them
# most reliably), and shared/mathfonts.cpp holds them as bytes. Usage: scripts/make-math-fonts.py latinmodern-math.otf
import sys, os
from fontTools.ttLib import TTFont, newTable
from fontTools.subset import Subsetter, Options
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen

src = sys.argv[1]
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "shared")

def rng(a, b): return list(range(a, b + 1))

UPRIGHT = (rng(0x20, 0x7E) + rng(0xA0, 0xFF) + [0x131, 0x237] + rng(0x2C6, 0x2DC) + rng(0x300, 0x308) + rng(0x391, 0x3A9)
           + rng(0x3B1, 0x3C9) + [0x3D1, 0x3D5, 0x3D6, 0x3F0, 0x3F1, 0x3F5] + [0x2016] + rng(0x2020, 0x2022) + [0x2026]
           + rng(0x2032, 0x2034) + [0x20D7, 0x2102, 0x2115, 0x211A, 0x211D, 0x2124] + rng(0x2190, 0x21FF) + rng(0x2200, 0x22FF)
           + rng(0x2308, 0x230B) + [0x27E8, 0x27E9])
ITALIC = {}   # plain code point -> the code point in Latin Modern Math whose glyph it gets
for i in range(26):
    ITALIC[0x41 + i] = 0x1D434 + i
    ITALIC[0x61 + i] = 0x210E if i == 7 else 0x1D44E + i   # (italic h is the Planck constant's)
for i in range(0x3B1, 0x3CA): ITALIC[i] = 0x1D6FC + (i - 0x3B1)
ITALIC.update({0x3F5: 0x1D716, 0x3D1: 0x1D717, 0x3F0: 0x1D718, 0x3D5: 0x1D719, 0x3F1: 0x1D71A, 0x3D6: 0x1D71B, 0x20: 0x20})
BOLD = {0x20: 0x20}
for i in range(26):
    BOLD[0x41 + i] = 0x1D400 + i
    BOLD[0x61 + i] = 0x1D41A + i
for i in range(10): BOLD[0x30 + i] = 0x1D7CE + i

def make(name, cmap_to_src, out):
    f = TTFont(src)
    best = f.getBestCmap()
    keep = sorted({cmap_to_src[u] for u in cmap_to_src if cmap_to_src[u] in best})
    opts = Options(); opts.layout_features = []; opts.hinting = False; opts.drop_tables += ["MATH", "GSUB", "GPOS", "FFTM"]
    opts.name_IDs = [0, 1, 2, 3, 4, 5, 6]; opts.notdef_outline = True
    sub = Subsetter(opts); sub.populate(unicodes=keep); sub.subset(f)
    best = f.getBestCmap()
    # the cmap: each plain code point to the glyph of its source code point
    newmap = {u: best[s] for u, s in cmap_to_src.items() if s in best}
    for t in f["cmap"].tables: t.cmap = {u: g for u, g in newmap.items() if u <= 0xFFFF or t.format == 12}
    f["cmap"].tables = [t for t in f["cmap"].tables if t.format in (4, 12)]
    # CFF outlines to TrueType ones
    gs = f.getGlyphSet(); order = f.getGlyphOrder()
    glyf = newTable("glyf"); glyf.glyphOrder = order; glyf.glyphs = {}
    for n in order:
        p = TTGlyphPen(gs); gs[n].draw(Cu2QuPen(p, 1.0, reverse_direction=True)); glyf[n] = p.glyph()
    f["glyf"] = glyf; f["loca"] = newTable("loca")
    for t in ("CFF ", "VORG"):
        if t in f: del f[t]
    m = newTable("maxp"); m.tableVersion = 0x00010000
    for k in ("numGlyphs", "maxPoints", "maxContours", "maxCompositePoints", "maxCompositeContours", "maxTwilightPoints",
              "maxStorage", "maxFunctionDefs", "maxInstructionDefs", "maxStackElements", "maxSizeOfInstructions",
              "maxComponentElements", "maxComponentDepth"): setattr(m, k, 0)
    m.maxZones = 1; f["maxp"] = m
    f["head"].glyphDataFormat = 0
    f["post"].formatType = 3.0
    f.sfntVersion = "\x00\x01\x00\x00"
    for rec in f["name"].names:   # its own name: not taken for a document's own (subset) Latin Modern Math
        if rec.nameID in (1, 4): rec.string = name.replace("-", " ")
        if rec.nameID == 6: rec.string = name
        if rec.nameID == 3: rec.string = name
    f.save(out)
    return os.path.getsize(out)

fonts = [("FeatherLMMath-Regular", {u: u for u in UPRIGHT}, "lmmath-upright.ttf"),
         ("FeatherLMMath-Italic", ITALIC, "lmmath-italic.ttf"),
         ("FeatherLMMath-Bold", BOLD, "lmmath-bold.ttf")]
cpp = ["// Made by scripts/make-math-fonts.py from Latin Modern Math (GUST Font License): do not edit.",
       '#include "mathfonts.hpp"', "namespace mathfonts {"]
for name, m, file in fonts:
    path = os.path.join(root, "fonts", file)
    size = make(name, m, path)
    data = open(path, "rb").read()
    var = file.split(".")[0].replace("-", "_")
    cpp.append("static const unsigned char %s[%d] = {" % (var, len(data)))
    for i in range(0, len(data), 24): cpp.append(",".join(str(b) for b in data[i:i + 24]) + ",")
    cpp.append("};")
    print(file, size)
cpp.append("const Font kFonts[3] = {{lmmath_upright, sizeof lmmath_upright}, {lmmath_italic, sizeof lmmath_italic}, {lmmath_bold, sizeof lmmath_bold}};")
cpp.append("} // namespace mathfonts")
open(os.path.join(root, "mathfonts.cpp"), "w").write("\n".join(cpp) + "\n")
