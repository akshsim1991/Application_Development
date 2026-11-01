"""Compose every video frame: scenes, overlays, panels, captions."""
import json, os, sys, math, hashlib
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import r3d

FPS = 20
W, H = 1280, 720
VX, VY, VW, VH = 24, 84, 840, 520          # viewport
PX, PY, PW, PH = 884, 84, 372, 520         # right panel
CACHE = "/tmp/c3d"; os.makedirs(CACHE, exist_ok=True)
FD = "/usr/share/fonts/opentype/inter/"
TIMING = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets/timing.json")))
DRAWING = Image.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets/drawing.png")).convert("RGBA")

INK = (15, 23, 42); GREY = (100, 116, 139); ACC = (21, 101, 192); GREEN = (22, 163, 74)
SKBLUE = (31, 111, 219); RED = (220, 70, 60); ORANGE = (234, 120, 20); TEAL = (13, 110, 125)

_fonts = {}
def font(size, bold=False, med=False, k=2):
    key = (size, bold, med, k)
    if key not in _fonts:
        f = "Inter-Bold.otf" if bold else ("Inter-Medium.otf" if med else "Inter-Regular.otf")
        _fonts[key] = ImageFont.truetype(FD + f, int(size * k))
    return _fonts[key]

def ease(x): x = min(max(x, 0.0), 1.0); return x * x * (3 - 2 * x)
def seg(p, a, b): return min(max((p - a) / (b - a), 0.0), 1.0)
def lerp(a, b, t): return a + (b - a) * t

# ----------------------------------------------------------------- drawing layer
class Lay:
    def __init__(self, w, h, k=2):
        self.k = k; self.w, self.h = w, h
        self.img = Image.new("RGBA", (w * k, h * k), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img, "RGBA")
    def _c(self, c, a=1.0):
        c = tuple(c); al = int((c[3] if len(c) == 4 else 255) * a)
        return (c[0], c[1], c[2], al)
    def line(self, pts, color, w=2, a=1.0, dash=None):
        k = self.k; P = [(x * k, y * k) for x, y in pts]
        if dash is None:
            self.d.line(P, fill=self._c(color, a), width=int(w * k), joint="curve")
        else:
            on, off = dash
            for (x0, y0), (x1, y1) in zip(P[:-1], P[1:]):
                L = math.hypot(x1 - x0, y1 - y0)
                n = max(int(L / ((on + off) * k)), 1); ux, uy = (x1 - x0) / L, (y1 - y0) / L
                s = 0
                while s < L:
                    e = min(s + on * k, L)
                    self.d.line([(x0 + ux * s, y0 + uy * s), (x0 + ux * e, y0 + uy * e)], fill=self._c(color, a), width=int(w * k))
                    s += (on + off) * k
    def arcpts(self, cx, cy, r, a0, a1, n=None):
        n = n or max(int(abs(a1 - a0) / 2), 8)
        return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy - r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
    def circle(self, cx, cy, r, color, w=2, a0=0, a1=360, a=1.0, dash=None, fill=None, fa=1.0):
        if fill is not None:
            pts = self.arcpts(cx, cy, r, 0, 360)
            self.poly(pts, fill, fa)
        if a1 - a0 <= 0.5: return
        self.line(self.arcpts(cx, cy, r, a0, a1), color, w, a, dash)
    def poly(self, pts, fill, a=1.0, outline=None, w=2):
        k = self.k
        self.d.polygon([(x * k, y * k) for x, y in pts], fill=self._c(fill, a))
        if outline is not None: self.line(list(pts) + [pts[0]], outline, w)
    def rrect(self, x0, y0, x1, y1, rad=8, fill=None, outline=None, w=1, a=1.0):
        k = self.k
        self.d.rounded_rectangle([x0 * k, y0 * k, x1 * k, y1 * k], rad * k,
                                 fill=self._c(fill, a) if fill is not None else None,
                                 outline=self._c(outline, a) if outline is not None else None, width=int(w * k))
    def text(self, x, y, s, size=20, color=INK, bold=False, med=False, anchor="la", a=1.0):
        self.d.text((x * self.k, y * self.k), s, font=font(size, bold, med, self.k), fill=self._c(color, a), anchor=anchor)
    def tw(self, s, size, bold=False, med=False): return font(size, bold, med, self.k).getlength(s) / self.k
    def pill(self, x, y, s, size=18, fg=INK, bg=(255, 255, 255), outline=(180, 195, 215), a=1.0, bold=True, padx=11, pady=6):
        w = self.tw(s, size, bold) + 2 * padx; h = size + 2 * pady
        self.rrect(x - w / 2, y - h / 2, x + w / 2, y + h / 2, h / 2, fill=bg, outline=outline, w=1.5, a=a)
        self.text(x, y + 0.5, s, size, fg, bold, anchor="mm", a=a)
    def arrow(self, p0, p1, color=INK, w=2, a=1.0, head=9, both=False):
        self.line([p0, p1], color, w, a)
        for (q, r) in ([(p1, p0)] + ([(p0, p1)] if both else [])):
            ang = math.atan2(q[1] - r[1], q[0] - r[0])
            for s in (-0.4, 0.4):
                self.line([q, (q[0] - head * math.cos(ang + s), q[1] - head * math.sin(ang + s))], color, w, a)
    def check(self, x, y, s=9, color=(255, 255, 255), w=3):
        self.line([(x - s * .55, y), (x - s * .1, y + s * .45), (x + s * .6, y - s * .45)], color, w)
    def done(self): return self.img.resize((self.w, self.h), Image.LANCZOS)

class Null:
    def __getattr__(self, n):
        if n == "arcpts": return lambda *a, **k: []
        return lambda *a, **k: (0 if n == "tw" else None)
    def done(self): return None

# ----------------------------------------------------------------- 3D cache
class R3:
    def __init__(self, dry=False): self.dry = dry; self.keys = set()
    def _get(self, M, az, el):
        q = lambda x: round(round(x / 0.25) * 0.25, 2)
        M = (q(M[0]), q(M[1]), q(M[2]), q(M[3]), int(M[4])); az = az % 360
        key = f"{M}_{az}_{el}"; fn = os.path.join(CACHE, hashlib.md5(key.encode()).hexdigest() + ".png")
        self.keys.add((M, az, el))
        if self.dry: return None
        if not os.path.exists(fn):
            r3d.render(M, az, el).save(fn)
        return Image.open(fn)
    def get(self, M, az=35, el=30, quant=3):
        if quant:
            az = az % 360; a0 = math.floor(az / quant) * quant; w = (az - a0) / quant
            a = self._get(M, a0, el)
            if w < 0.02 or self.dry: return a
            b = self._get(M, a0 + quant, el)
            return Image.blend(a, b, w)
        return self._get(M, az, el)

# ----------------------------------------------------------------- base page
def make_base():
    im = Image.new("RGBA", (W, H), (238, 242, 247, 255))
    d = ImageDraw.Draw(im, "RGBA")
    d.rectangle([0, 0, W, 68], fill=(255, 255, 255, 255)); d.line([(0, 68), (W, 68)], fill=(214, 222, 232, 255), width=1)
    # viewport gradient
    g = Image.new("RGBA", (VW, VH))
    gd = ImageDraw.Draw(g)
    for y in range(VH):
        t = y / VH; c = tuple(int(lerp(a, b, t)) for a, b in zip((222, 232, 243), (250, 252, 254))) + (255,)
        gd.line([(0, y), (VW, y)], fill=c)
    m = Image.new("L", (VW * 2, VH * 2), 0); ImageDraw.Draw(m).rounded_rectangle([0, 0, VW * 2 - 1, VH * 2 - 1], 28, fill=255)
    m = m.resize((VW, VH), Image.LANCZOS); im.paste(g, (VX, VY), m)
    # shadows + cards
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0)); sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle([PX, PY + 4, PX + PW, PY + PH + 4], 16, fill=(30, 41, 59, 40))
    sh = sh.filter(ImageFilter.GaussianBlur(7)); im.alpha_composite(sh)
    ov = Lay(W, H); ov.rrect(PX, PY, PX + PW, PY + PH, 16, fill=(255, 255, 255), outline=(214, 222, 232), w=1)
    ov.rrect(VX, VY, VX + VW, VY + VH, 14, outline=(200, 212, 226), w=1.5)
    im.alpha_composite(ov.done())
    return im
BASE = None

# ----------------------------------------------------------------- sketch helpers
SC = 3.6; CX, CY = VW / 2, VH / 2 + 10
def sx(mm): return CX + SC * mm
def sy(mm): return CY - SC * mm

def sketch_grid(v, a=1.0, faint=1.0):
    for mm in range(-100, 101, 10):
        c = (205, 215, 228) if mm % 50 else (186, 199, 216)
        v.line([(sx(mm), 0), (sx(mm), VH)], c, 1, a * faint); v.line([(0, sy(mm)), (VW, sy(mm))], c, 1, a * faint)
    v.line([(0, sy(0)), (VW, sy(0))], RED, 1.8, a * .85); v.line([(sx(0), 0), (sx(0), VH)], (60, 170, 90), 1.8, a * .85)
    v.circle(sx(0), sy(0), 4, (30, 41, 59), 2, fill=(255, 255, 255), a=a)

def dim_dia(v, r_mm, ang, txt, a=1.0, col=TEAL, glow=0):
    r = r_mm * SC; c = math.radians(ang)
    p0 = (sx(0) - r * math.cos(c), sy(0) + r * math.sin(c)); p1 = (sx(0) + r * math.cos(c), sy(0) - r * math.sin(c))
    v.arrow(p0, p1, col, 2 + glow, a, both=True)
    v.pill(sx(0) + 0.5 * r * math.cos(c) + 4, sy(0) - 0.5 * r * math.sin(c) - 2, txt, 19, col, (255, 255, 255), col if glow else (170, 190, 210), a)

def mm_label(v, x, y, txt, a=1.0, col=TEAL): v.pill(x, y, txt, 18, col, (255, 255, 255), (170, 190, 210), a)

# ----------------------------------------------------------------- panel items
PANEL = {
 "intro": [[], [("tick", "Flange Ø120 × 16")], [("tick", "Hub Ø60 × 40"), ("tick", "Bore Ø30 + key slot"), ("tick", "4 × Ø12 bolt holes")],
           [("tick", "Sketch · Extrude · Cut"), ("tick", "Hole · Circular Pattern")], []],
 "read": [[], [("val", "Flange Ø", "120"), ("val", "Flange thickness", "16")],
          [("val", "Hub Ø", "60"), ("val", "Total height", "40"), ("val", "Hub above flange", "24")],
          [("val", "Bore Ø", "30"), ("val", "Key slot width", "8")],
          [("val", "Bore bottom → slot top", "33.3"), ("val", "Slot extra depth", "3.3")],
          [("val", "Bolt holes", "4 × Ø12"), ("val", "Pitch circle (PCD)", "Ø90")]],
 "setup": [[("cmd", "File", "New Design")], [("val", "Units", "mm"), ("cmd", "Ctrl+S", "Save: Flanged Hub")], [("val", "Sketch plane", "XY")]],
 "sketch1": [[("cmd", "Create", "Create Sketch → XY plane")], [("cmd", "C", "Center Diameter Circle"), ("val", "Diameter", "120 mm")],
             [("val", "Second circle Ø", "60 mm")], [("cmd", "D", "Sketch Dimension")], [("cmd", "✓", "Finish Sketch")]],
 "extrude": [[("cmd", "E", "Extrude")],
             [("reset",), ("val", "Profiles", "ring + disc"), ("val", "Distance", "16 mm"), ("val", "Operation", "New Body")],
             [("reset",), ("val", "Profile", "Ø60 disc"), ("val", "Distance", "40 mm"), ("val", "Operation", "Join")], []],
 "boresketch": [[("cmd", "Create", "Sketch on hub top face")], [("cmd", "C", "Circle"), ("val", "Diameter", "30 mm")],
                [("cmd", "R", "Rectangle"), ("val", "Slot width", "8 mm"), ("val", "Slot top (Y)", "18.3 mm")], [("val", "15 + 3.3", "= 18.3")],
                [("cmd", "T", "Trim inner lines"), ("cmd", "✓", "Finish Sketch")]],
 "cut": [[("cmd", "E", "Extrude"), ("val", "Profile", "bore + slot")], [("val", "Extent", "All"), ("val", "Operation", "Cut")], []],
 "holesketch": [[("cmd", "Create", "Sketch on flange top face")], [("cmd", "C", "Circle"), ("val", "Diameter", "90 mm"), ("cmd", "X", "Construction")],
                [("cmd", "P", "Point on Y axis"), ("val", "Distance from origin", "45 mm"), ("cmd", "✓", "Finish Sketch")]],
 "hole": [[("cmd", "H", "Hole"), ("val", "Placement", "Sketch point"), ("val", "Diameter", "12 mm"), ("val", "Extent", "All")],
          [("reset",), ("cmd", "Create", "Pattern → Circular Pattern")],
          [("val", "Object type", "Features"), ("val", "Object", "Hole1"), ("val", "Axis", "Hub cylinder"), ("val", "Quantity", "4"), ("val", "Angle", "360°")], []],
 "finish": [[("reset",), ("cmd", "Inspect", "Measure")], [("tick", "Flange Ø120 × 16"), ("tick", "Hub Ø60 × 40"), ("tick", "Material: Steel")],
            [("tick", "Bore Ø30 + 8 mm key slot"), ("tick", "4 × Ø12 on PCD Ø90")], [("note", "Edit a dimension in the timeline and the model updates.")], []],
}
PANEL_TITLE = {"intro": "You will build", "read": "Key dimensions", "finish": "Final check"}

FEATS = [("Sketch1", "sk", 3, 4), ("Extrude1", "ex", 4, 1), ("Extrude2", "ex", 4, 2), ("Sketch2", "sk", 5, 4),
         ("Extrude3", "cu", 6, 2), ("Sketch3", "sk", 7, 2), ("Hole1", "ho", 8, 0), ("Pattern1", "pa", 8, 2)]

def wrap(lay, s, size, maxw, bold=False):
    words, lines, cur = s.split(), [], ""
    for w_ in words:
        t = (cur + " " + w_).strip()
        if lay.tw(t, size, bold) <= maxw: cur = t
        else: lines.append(cur); cur = w_
    if cur: lines.append(cur)
    return lines

def draw_panel(f, sid, si, i, p):
    title = PANEL_TITLE.get(sid, "Commands & settings")
    f.text(PX + 24, PY + 22, title.upper(), 15, GREY, bold=True)
    f.line([(PX + 24, PY + 50), (PX + PW - 24, PY + 50)], (225, 231, 240), 1.5)
    items = []
    for j, lst in enumerate(PANEL[sid][:i + 1]):
        for it in lst:
            if it[0] == "reset": items = []
            else: items.append((it, j))
    y = PY + 66
    for it, j in items:
        a = 1.0 if j < i else ease(seg(p, 0.0, 0.25))
        dy = (1 - a) * 8
        t = it[0]; yy = y + dy
        if t == "cmd":
            kw = max(f.tw(it[1], 17, True) + 22, 40)
            f.rrect(PX + 24, yy, PX + 24 + kw, yy + 36, 8, fill=(30, 41, 59), a=a)
            f.text(PX + 24 + kw / 2, yy + 18.5, it[1], 17, (255, 255, 255), True, anchor="mm", a=a)
            f.text(PX + 24 + kw + 12, yy + 18.5, it[2], 19, INK, med=True, anchor="lm", a=a); y += 50
        elif t == "val":
            f.text(PX + 24, yy + 18, it[1], 17, GREY, anchor="lm", a=a)
            f.text(PX + PW - 24, yy + 18, it[2], 20, ACC, True, anchor="rm", a=a); y += 38
        elif t == "tick":
            f.circle(PX + 42, yy + 18, 13, GREEN, 1, fill=GREEN, fa=a)
            f.check(PX + 42, yy + 18, 9, w=2.6)
            f.text(PX + 66, yy + 18.5, it[1], 19, INK, med=True, anchor="lm", a=a); y += 48
        elif t == "note":
            for ln in wrap(f, it[1], 18, PW - 56):
                f.text(PX + 24, yy, ln, 18, GREY, anchor="la", a=a); yy += 26
            y += 60

def draw_timeline(f, si, i, pulse=0):
    chips = [(n, k) for n, k, s, sent in FEATS if (si > s or (si == s and i >= sent))]
    if not chips: return
    x = 18; y = VH - 40
    f.rrect(8, y - 6, 8 + 16 + sum(f.tw(n, 13, True) + 44 for n, _ in chips), y + 32, 12, fill=(255, 255, 255), outline=(200, 212, 226), w=1, a=0.95)
    cols = {"sk": (46, 125, 220), "ex": (110, 125, 145), "cu": (225, 90, 70), "ho": (225, 90, 70), "pa": (130, 100, 200)}
    for n, k in chips:
        f.rrect(x, y + 2, x + 22, y + 24, 5, fill=cols[k])
        if k == "sk": f.line([(x + 6, y + 18), (x + 16, y + 8)], (255, 255, 255), 2)
        elif k in ("ex", "cu"): f.rrect(x + 5, y + 7, x + 17, y + 19, 2, outline=(255, 255, 255), w=1.8)
        else: f.circle(x + 11, y + 13, 5, (255, 255, 255), 1.8)
        f.text(x + 29, y + 13.5, n, 13, INK, True, anchor="lm"); x += f.tw(n, 13, True) + 44

# ----------------------------------------------------------------- scenes
FULL = (16, 40, 40, 17, 4)
def ellipse_pts(r, z, az, el, S=4.4, n=96, cx=VW / 2, cy=VH / 2):
    th = np.linspace(0, 2 * np.pi, n)
    pts = np.stack([r * np.cos(th), r * np.sin(th), np.full(n, z)], 1)
    return [tuple(p) for p in r3d.project(pts, az, el, S, cx, cy)]
def P3(pt, az=35, el=30): return tuple(r3d.project(np.array([pt], np.float32), az, el, 4.4, VW / 2, VH / 2)[0])

def top_view_part(v, with_holes=False, show_bore=True, flange_hi=0.0):
    v.circle(sx(0), sy(0), 60 * SC, (90, 105, 125), 2.2, fill=(206, 216, 228), fa=1)
    if flange_hi:
        ring = v.arcpts(sx(0), sy(0), 60 * SC, 0, 360) + v.arcpts(sx(0), sy(0), 30 * SC, 360, 0)
        v.poly(ring, SKBLUE, 0.28 * flange_hi)
    v.circle(sx(0), sy(0), 30 * SC, (90, 105, 125), 2.2, fill=(232, 238, 245), fa=1)
    if show_bore:
        v.circle(sx(0), sy(0), 15 * SC, (255, 255, 255), 1, fill=(250, 252, 254))
        pts = slot_profile_pts(v); v.line(pts + [pts[0]], (20, 30, 45), 2.4)

def slot_profile_pts(v):
    pts = [(sx(4), sy(14.457)), (sx(4), sy(18.3)), (sx(-4), sy(18.3)), (sx(-4), sy(14.457))]
    pts += v.arcpts(sx(0), sy(0), 15 * SC, 105.54, 434.46, 120)
    return pts

def scene_draw(sid, si, i, p, u, t, tt, v, f, r3, base_layer):
    """returns the 3D image (or None).  v = viewport overlay, f = frame overlay (1x frame coords)"""
    az_turn = 35 + 9 * t
    img3 = None
    if sid in ("intro", "finish"):
        img3 = r3.get(FULL, az_turn, 30)
        if sid == "intro":
            a = ease(seg(tt, 0.2, 1.4))
            v.text(VW / 2, 40, "Design a Flanged Hub", 38, INK, True, anchor="mm", a=a)
            v.text(VW / 2, 74, "Autodesk Fusion · Step-by-step tutorial", 19, GREY, med=True, anchor="mm", a=a)
        else:
            if i == 4:
                a = ease(seg(p, 0.0, 0.5))
                v.text(VW / 2, 52, "Thanks for watching!", 38, INK, True, anchor="mm", a=a)
            if i == 0:
                for (px_, py_, lab) in [((0, 60, 16), 0, "Ø120"), ((30, 0, 40), 0, "Ø60 × 40")]:
                    pass
                pos = P3((45, 0, 16), az_turn); v.pill(pos[0], pos[1] - 36, "Measure ↔ compare", 18, ACC, (255, 255, 255), ACC, a=ease(seg(p, .1, .3)))
    elif sid == "read":
        s = min(VW / DRAWING.width, VH / DRAWING.height) * 0.98
        dw, dh = int(DRAWING.width * s), int(DRAWING.height * s)
        img3 = Image.new("RGBA", (VW, VH), (0, 0, 0, 0))
        img3.paste(DRAWING.resize((dw, dh), Image.LANCZOS), (int((VW - dw) / 2), int((VH - dh) / 2 + 2)))
        ox, oy = int((VW - dw) / 2), int((VH - dh) / 2 + 2)
        HI = {1: [(865, 95, 925, 675), (985, 52, 1090, 100), (20, 640, 105, 675)],
              2: [(1255, 240, 1305, 535), (990, 700, 1195, 745), (520, 618, 640, 650)],
              3: [(1205, 305, 1250, 470), (365, 245, 435, 295)],
              4: [(1305, 295, 1360, 465)],
              5: [(685, 495, 915, 530), (635, 92, 755, 124)]}
        for (x0, y0, x1, y1) in HI.get(i, []):
            a = 0.6 + 0.4 * math.sin(tt * 6)
            v.rrect(ox + x0 * s, oy + y0 * s, ox + x1 * s, oy + y1 * s, 8, fill=(255, 200, 0), outline=ORANGE, w=2.5, a=0.30)
            v.rrect(ox + x0 * s, oy + y0 * s, ox + x1 * s, oy + y1 * s, 8, outline=ORANGE, w=3, a=a)
    elif sid == "setup":
        # mock browser panel + canvas
        ph = 360
        v.rrect(16, 16, 266, 16 + ph, 12, fill=(255, 255, 255), outline=(200, 212, 226), w=1.5)
        v.text(30, 30, "BROWSER", 13, GREY, True)
        rows = [("Document Settings", 0, "r"), ("Units: mm", 2, None), ("Named Views", 0, "r"), ("Origin", 0, "d"),
                ("XY plane", 2, None), ("XZ plane", 2, None), ("YZ plane", 2, None), ("X / Y / Z axes", 2, None)]
        y = 62
        for (txt, lv, mk) in rows:
            hi = (txt.startswith("Units") and i == 1) or (txt.startswith("XY") and i == 2)
            if hi: v.rrect(24, y - 5, 258, y + 25, 6, fill=(21, 101, 192), a=0.14 + 0.1 * math.sin(tt * 6))
            vis = (txt not in ("Units: mm",) or i >= 1) and (lv < 2 or i >= 2 or txt.startswith("Units"))
            if vis:
                x0 = 34 if lv == 0 else 56
                if mk == "r": v.poly([(34, y + 4), (34, y + 16), (43, y + 10)], GREY)
                if mk == "d": v.poly([(33, y + 6), (45, y + 6), (39, y + 15)], GREY)
                v.text(x0 + (16 if mk else 0), y, txt, 17, ACC if hi else INK, hi)
            y += 36
        # canvas content
        if i >= 1:
            tabw = 230; v.rrect(300, 22, 300 + tabw, 58, 10, fill=(255, 255, 255), outline=(200, 212, 226), w=1.5)
            name = "Flanged Hub v1"[: int(len("Flanged Hub v1") * (ease(seg(p, 0.35, 0.8)) if i == 1 else 1))]
            v.text(318, 40, name, 17, INK, True, anchor="lm")
        else:
            a = ease(seg(p, 0.1, 0.5))
            v.text(580, 250, "New Design", 38, GREY, True, anchor="mm", a=a)
            v.text(580, 290, "empty canvas · millimetres", 19, GREY, anchor="mm", a=a)
        if i == 1 and p > 0.5:
            v.pill(580, 440, "Units: mm   ·   Saved ✓", 20, GREEN, (255, 255, 255), GREEN, a=ease(seg(p, .5, .7)))
        if i == 2:
            az, el = 35, 30; c0 = (610, 250)
            def pr(x_, y_, z_): return tuple(r3d.project(np.array([[x_, y_, z_ + 14]], np.float32), az, el, 2.7, *c0)[0])
            a = ease(seg(p, 0, .3))
            sq = [pr(-60, -60, 0), pr(60, -60, 0), pr(60, 60, 0), pr(-60, 60, 0)]
            v.poly(sq, SKBLUE, 0.22 * a, outline=SKBLUE, w=2)
            for pts, col in [((pr(-80, 0, 0), pr(80, 0, 0)), RED), ((pr(0, -80, 0), pr(0, 80, 0)), (60, 170, 90)), ((pr(0, 0, -5), pr(0, 0, 100)), (50, 90, 220))]:
                v.line(list(pts), col, 2.5, a)
            v.pill(sq[1][0] - 30, sq[1][1] + 26, "XY plane", 18, ACC, (255, 255, 255), ACC, a=a)
    elif sid == "sketch1":
        g = ease(seg(tt, 0, 0.8)) if i == 0 else 1
        sketch_grid(v, g)
        con = (i == 4 and p > 0.3)
        col = (20, 20, 20) if con else SKBLUE
        if i == 0:
            v.pill(VW / 2, 26, "Sketch on the XY plane", 22, ACC, (255, 255, 255), ACC, a=g)
        if i >= 1:
            pr_ = ease(seg(p, 0.30, 0.85)) if i == 1 else 1
            v.circle(sx(0), sy(0), 60 * SC, col, 2.8, 0, 360 * pr_)
            if i > 1 or p > .8: dim_dia(v, 60, 150, "Ø120", a=ease(seg(p, .8, 1)) if i == 1 else 1, glow=(1.5 if i == 3 else 0))
        if i >= 2:
            pr_ = ease(seg(p, 0.3, 0.8)) if i == 2 else 1
            v.circle(sx(0), sy(0), 30 * SC, col, 2.8, 0, 360 * pr_)
            if i > 2 or p > .75: dim_dia(v, 30, 30, "Ø60", a=ease(seg(p, .75, 1)) if i == 2 else 1, glow=(1.5 if i == 3 else 0))
        if i == 4:
            a = ease(seg(p, 0.35, 0.55))
            v.pill(VW / 2, 26, "Fully constrained", 20, (20, 20, 20), (255, 255, 255), (20, 20, 20), a=a)
            bx = VW - 180; v.rrect(bx, 462, bx + 164, 504, 10, fill=GREEN if (math.sin(tt * 7) > 0) else (18, 140, 64))
            v.check(bx + 24, 483, 9, w=3); v.text(bx + 94, 483, "Finish Sketch", 15, (255, 255, 255), True, anchor="mm")
    elif sid == "extrude":
        az, el = 35, 30
        if i == 0:
            M = (0, 0, 0, 0, 0)
            img3 = r3.get(M, az, el)
            o = ellipse_pts(60, 0, az, el); n_ = ellipse_pts(30, 0, az, el)
            v.poly(o, SKBLUE, 0.20); v.poly(n_, SKBLUE, 0.25)
            v.line(o + [o[0]], (20, 20, 20), 2.5); v.line(n_ + [n_[0]], (20, 20, 20), 2.5)
            v.pill(VW / 2, 26, "Two profiles from one sketch", 21, ACC, (255, 255, 255), ACC, a=ease(seg(p, .1, .4)))
        else:
            tf = 16 * ease(seg(p, 0.12, 0.8)) if i == 1 else 16
            th = 40 * ease(seg(p, 0.12, 0.85)) if i == 2 else (40 if i == 3 else 0)
            img3 = r3.get((tf, th, 0, 0, 0), az, el)
            if i == 1:
                pos = P3((62, 0, tf), az); v.pill(pos[0] + 60, pos[1], f"{tf:4.1f} mm", 20, ACC, (255, 255, 255), ACC)
                v.arrow((pos[0] + 22, pos[1]), P3((62, 0, tf), az), ACC, 2)
            if i == 2:
                pos = P3((0, -30, th), az); v.pill(pos[0] - 120, pos[1] - 10, f"{th:4.1f} mm · Join", 20, ACC, (255, 255, 255), ACC)
                v.arrow((pos[0] - 62, pos[1] - 10), pos, ACC, 2)
            if i == 3:
                a = ease(seg(p, 0.1, 0.4))
                b0, b1, b2 = P3((70, 0, 0)), P3((70, 0, 16)), P3((70, 0, 40))
                v.arrow(b0, b2, TEAL, 2, a, both=True)
                v.pill(b2[0] + 36, (b0[1] + b2[1]) / 2 - 18, "40", 20, TEAL, (255, 255, 255), TEAL, a=a)
                v.pill(b1[0] + 36, b1[1] + 22, "16", 20, TEAL, (255, 255, 255), TEAL, a=a)
    elif sid == "boresketch":
        sketch_grid(v, 1.0, 0.55)
        top_view_part(v, show_bore=False)
        if i == 0:
            ring = v.arcpts(sx(0), sy(0), 30 * SC, 0, 360)
            v.poly(ring, SKBLUE, 0.22 + 0.1 * math.sin(tt * 6))
            v.pill(VW / 2, 26, "New sketch on the hub's top face", 21, ACC, (255, 255, 255), ACC)
        col = (20, 20, 20) if i == 4 and p > 0.6 else SKBLUE
        if i >= 1:
            pr_ = ease(seg(p, .2, .8)) if i == 1 else 1
            v.circle(sx(0), sy(0), 15 * SC, SKBLUE if i < 4 else col, 2.8, 0, 360 * pr_)
            if i > 1 or p > .75: dim_dia(v, 15, 215, "Ø30", a=ease(seg(p, .75, 1)) if i == 1 else 1)
        if i >= 2 and i < 4 or (i == 4 and p < 0.6):
            pr_ = ease(seg(p, .1, .7)) if i == 2 else 1
            pts = [(sx(-4), sy(0)), (sx(4), sy(0)), (sx(4), sy(18.3)), (sx(-4), sy(18.3)), (sx(-4), sy(0))]
            n = max(int(pr_ * 4 * 100), 0); L = []
            tot = 0
            for a_, b_ in zip(pts[:-1], pts[1:]):
                for s_ in np.linspace(0, 1, 26)[:-1]: L.append((lerp(a_[0], b_[0], s_), lerp(a_[1], b_[1], s_)))
            L.append(pts[-1]); L = L[: max(int(len(L) * pr_), 2)]
            v.line(L, SKBLUE, 2.8)
            if i > 2 or p > .7:
                a = 1 if i > 2 else ease(seg(p, .7, 1))
                mm_label(v, sx(0), sy(18.3) - 22, "8", a); v.arrow((sx(-4), sy(18.3) - 8), (sx(4), sy(18.3) - 8), TEAL, 2, a, 6, True)
                mm_label(v, sx(24), sy(18.3), "18.3", a); v.line([(sx(4), sy(18.3)), (sx(16), sy(18.3))], TEAL, 1.5, a, (4, 3))
        if i == 3:
            a = ease(seg(p, 0.1, 0.4)); xx = sx(-32)
            v.arrow((xx, sy(-15)), (xx, sy(18.3)), TEAL, 2.2, a, both=True)
            v.line([(xx, sy(0)), (sx(0), sy(0))], TEAL, 1.2, a, (4, 3))
            v.line([(xx - 8, sy(-15)), (sx(-8), sy(-15))], TEAL, 1.2, a, (4, 3)); v.line([(xx - 8, sy(18.3)), (sx(-4), sy(18.3))], TEAL, 1.2, a, (4, 3))
            mm_label(v, xx - 36, (sy(-15) + sy(18.3)) / 2, "33.3", a)
            v.pill(sx(0), 470, "15  +  3.3  =  18.3", 22, ACC, (255, 255, 255), ACC, a=ease(seg(p, .4, .7)))
            v.text(xx + 12, sy(-7.5), "15", 16, TEAL, True, anchor="lm", a=a); v.text(xx + 12, sy(9), "18.3", 16, TEAL, True, anchor="lm", a=a)
        if i == 4:
            if p < 0.6:
                a = 0.5 + 0.5 * math.sin(tt * 10)
                v.line([(sx(-4), sy(0)), (sx(4), sy(0))], RED, 3.5, a, (7, 5)); v.line([(sx(4), sy(0)), (sx(4), sy(14.457))], RED, 3.5, a, (7, 5))
                v.line([(sx(-4), sy(0)), (sx(-4), sy(14.457))], RED, 3.5, a, (7, 5))
                v.pill(VW / 2, 26, "Trim the lines inside the circle", 21, RED, (255, 255, 255), RED)
            else:
                pts = slot_profile_pts(v); v.poly(pts, SKBLUE, 0.28 * ease(seg(p, .6, .8)))
                v.line(pts + [pts[0]], (20, 20, 20), 2.8)
                v.pill(VW / 2, 26, "One closed profile", 21, (20, 20, 20), (255, 255, 255), (20, 20, 20), a=ease(seg(p, .65, .85)))
    elif sid == "cut":
        az, el = 35, 30
        if i < 2:
            img3 = r3.get((16, 40, 0, 0, 0), az, el)
            pts = [tuple(q) for q in r3d.project(np.array([(x_ , y_, 40) for x_, y_ in
                   [(4, 14.457), (4, 18.3), (-4, 18.3), (-4, 14.457)] + [(15 * math.cos(math.radians(a_)), 15 * math.sin(math.radians(a_))) for a_ in np.linspace(105.54, 434.46, 100)]], np.float32), az, el, 4.4, VW / 2, VH / 2)]
            v.poly(pts, SKBLUE, 0.45 + 0.15 * math.sin(tt * 6)); v.line(pts + [pts[0]], (20, 20, 20), 2.5)
            b = P3((0, 0, 40));
            if i == 1:
                a = ease(seg(p, .1, .4)); v.arrow((b[0], b[1] - 10), (b[0], b[1] + 150), ORANGE, 4, a, 14)
                v.pill(b[0] + 85, b[1] + 80, "Extent: All", 20, ORANGE, (255, 255, 255), ORANGE, a=a)
        else:
            dc = 40 * ease(seg(p, 0.08, 0.8))
            img3 = r3.get((16, 40, dc, 0, 0), az, el)
            if p > 0.8: v.pill(VW / 2, 26, "Bore + key slot in one cut", 21, ACC, (255, 255, 255), ACC, a=ease(seg(p, .8, 1)))
    elif sid == "holesketch":
        sketch_grid(v, 1.0, 0.5)
        top_view_part(v, flange_hi=(0.6 + 0.4 * math.sin(tt * 6)) if i == 0 else 0.0)
        if i == 0: v.pill(VW / 2, 26, "New sketch on the flange's top face", 21, ACC, (255, 255, 255), ACC)
        if i >= 1:
            pr_ = ease(seg(p, .15, .75)) if i == 1 else 1
            v.circle(sx(0), sy(0), 45 * SC, ORANGE if i == 1 else (140, 90, 30), 2.4, 0, 360 * pr_, dash=(14, 5))
            if i > 1 or p > .7:
                a = ease(seg(p, .7, 1)) if i == 1 else 1; dim_dia(v, 45, 200, "Ø90", a=a)
                if i == 1: v.pill(VW / 2, 26, "X  →  construction line (pitch circle)", 21, ORANGE, (255, 255, 255), ORANGE, a=a)
        if i == 2:
            a = ease(seg(p, .1, .35))
            v.circle(sx(0), sy(45), 7, (20, 20, 20), 2.2, fill=SKBLUE, a=a)
            v.arrow((sx(24), sy(0)), (sx(24), sy(45)), TEAL, 2, a, 8, True); mm_label(v, sx(24) + 24, sy(22), "45", a)
            v.line([(sx(0), sy(45)), (sx(24) + 8, sy(45))], TEAL, 1.2, a, (4, 3))
            v.pill(VW / 2, 26, "Point on the Y axis", 21, ACC, (255, 255, 255), ACC, a=a)
    elif sid == "hole":
        az, el = 35, 30
        if i == 0:
            dh = 17 * ease(seg(p, 0.12, 0.82))
            img3 = r3.get((16, 40, 40, dh, 1), az, el)
            pos = P3((0, 45, 16)); a = ease(seg(p, .0, .2))
            v.pill(pos[0] + 120, pos[1] - 50, "Ø12 · Extent: All", 20, ORANGE, (255, 255, 255), ORANGE, a=a)
            v.arrow((pos[0] + 62, pos[1] - 44), (pos[0] + 8, pos[1] - 4), ORANGE, 2.5, a)
        elif i == 1:
            img3 = r3.get((16, 40, 40, 17, 1), az, el)
            a = ease(seg(p, .15, .4))
            for (hx, hy) in r3d.HOLES[1:]:
                q = P3((hx, hy, 16)); v.circle(q[0], q[1], 12, SKBLUE, 2, a=a * (0.5 + 0.5 * math.sin(tt * 6)), dash=(5, 4))
            v.pill(VW / 2, 26, "Create → Pattern → Circular Pattern", 21, ACC, (255, 255, 255), ACC)
        elif i == 2:
            n = 1 if p < 0.45 else (2 if p < 0.58 else (3 if p < 0.70 else 4))
            img3 = r3.get((16, 40, 40, 17, n), az, el)
            v.pill(VW / 2, 26, f"Quantity: {n}", 22, ACC, (255, 255, 255), ACC)
            c = P3((0, 0, 40))
            if p > 0.3: v.arrow((c[0], c[1] - 40), (c[0], c[1] + 6), ORANGE, 2.5, ease(seg(p, .3, .45)))
        else:
            img3 = r3.get(FULL, az_turn, 30)
            a = ease(seg(p, .1, .3))
            v.pill(VW / 2, 26, "4 × Ø12  EQS  on  PCD Ø90", 22, ACC, (255, 255, 255), ACC, a=a)
    return img3

def timeline_state(t):
    for si, sc in enumerate(TIMING):
        if t < sc["end"] or si == len(TIMING) - 1:
            i, p = 0, 0.0
            for k, (s, a, b) in enumerate(sc["caps"]):
                if t >= a:
                    i = k; p = min((t - a) / max(b - a, 1e-3), 1.0)
            tt = t - sc["start"]; u = tt / (sc["end"] - sc["start"])
            return si, sc, i, p, u, tt
    return len(TIMING) - 1, TIMING[-1], 0, 0, 0, 0

def compose(t, r3, dry=False):
    global BASE
    if BASE is None: BASE = make_base()
    si, sc, i, p, u, tt = timeline_state(t)
    sid = sc["id"]
    v = Null() if dry else Lay(VW, VH); f = Null() if dry else Lay(W, H)
    img3 = scene_draw(sid, si, i, p, u, t, tt, v, f, r3, None)
    if dry: return None
    frame = BASE.copy()
    # header
    f.rrect(24, 17, 58, 51, 9, fill=ACC); f.text(41, 34.5, "F", 20, (255, 255, 255), True, anchor="mm")
    f.text(72, 34, sc["title"], 26, INK, True, anchor="lm")
    f.text(W - 24, 34, "Autodesk Fusion · Flanged Hub", 17, GREY, med=True, anchor="rm")
    total = TIMING[-1]["end"]; f.rrect(0, 65, W * t / total, 69, 0, fill=ACC)
    # viewport content
    if img3 is not None:
        sh = Image.new("RGBA", (VW, VH), (0, 0, 0, 0)); sd = ImageDraw.Draw(sh)
        if sid not in ("read",):
            sd.ellipse([VW / 2 - 250, VH / 2 + 122, VW / 2 + 250, VH / 2 + 225], fill=(30, 41, 59, 55))
            frame_vp = sh.filter(ImageFilter.GaussianBlur(14))
        layer = Image.new("RGBA", (VW, VH), (0, 0, 0, 0))
        if sid not in ("read",) and not (sid in ("extrude",) and i == 0) and si >= 0 and (sid not in ("cut",) or True):
            layer.alpha_composite(frame_vp)
        layer.alpha_composite(img3)
        vp_base = frame.crop((VX, VY, VX + VW, VY + VH)); vp_base.alpha_composite(layer)
        frame.paste(vp_base, (VX, VY))
    vo = v.done()
    m = Image.new("L", (VW * 2, VH * 2), 0); ImageDraw.Draw(m).rounded_rectangle([0, 0, VW * 2 - 1, VH * 2 - 1], 26, fill=255)
    m = m.resize((VW, VH), Image.LANCZOS)
    vo2 = Image.new("RGBA", (VW, VH), (0, 0, 0, 0)); vo2.paste(vo, (0, 0), m)
    frame.alpha_composite(vo2, (VX, VY))
    # timeline + panel (frame layer, offset into viewport)
    tl = Lay(VW, VH); draw_timeline(tl, si, i); frame.alpha_composite(tl.done(), (VX, VY))
    draw_panel(f, sid, si, i, p)
    # caption
    caps = sc["caps"]; cur = None
    for (s, a, b) in caps:
        if t >= a: cur = s
    if cur and t < TIMING[-1]["end"] - 1.2:
        lines = wrap(f, cur, 27, 1100, False); hh = 22 + 36 * len(lines)
        y0 = H - 16 - hh
        f.rrect(W / 2 - 560, y0, W / 2 + 560, H - 16, 18, fill=(15, 23, 42), a=0.9)
        for k, ln in enumerate(lines):
            f.text(W / 2, y0 + 12 + 36 * k + 18, ln, 27, (255, 255, 255), med=True, anchor="mm")
    frame.alpha_composite(f.done())
    return frame.convert("RGB")

if __name__ == "__main__":
    ts = [float(x) for x in sys.argv[1:]]
    r3 = R3()
    for t in ts:
        compose(t, r3).save(f"/tmp/prev_{t:06.1f}.png"); print("saved", t)
