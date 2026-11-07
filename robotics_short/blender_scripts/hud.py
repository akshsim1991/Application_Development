"""2-D overlay layer (titles, labels, matrix typography, captions).

Rendered in Pillow/matplotlib on top of the Blender frames so that equations stay
razor sharp (never touched by depth-of-field or motion blur).  Every angle shown
comes from motion.theta_z_deg()/demo_angle(), the same functions that drive the
Blender animation, so matrix and 3D rotation cannot drift apart.
Design grid: 1080 x 1920 (scaled to the output size).
"""
import io
import math
import os
from functools import lru_cache
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import matplotlib
matplotlib.use("Agg")
from matplotlib import mathtext
from matplotlib.font_manager import FontProperties
from config import *    # noqa
import motion
import subtitles

WHITE = (255, 255, 255)
GREY = (170, 178, 190)
ORANGE = (255, 140, 20)
RED, GREEN, BLUE = (255, 70, 60), (60, 215, 90), (70, 130, 255)
PANEL = (8, 10, 14)

_F = {}


def font(weight, size):
    k = (weight, size)
    if k not in _F:
        fp = path("assets", "fonts", f"Inter-{weight}.otf")
        try:
            _F[k] = ImageFont.truetype(fp, size)
        except Exception:
            _F[k] = ImageFont.truetype("DejaVuSans-Bold.ttf", size)
    return _F[k]


def fade(f, a, b, fi=10, fo=10):
    """alpha: ramps in over fi frames after a, out over fo frames before b."""
    if f < a or f >= b:
        return 0.0
    x = 1.0
    if fi and f < a + fi:
        x = min(x, motion.ease((f - a) / float(fi)))
    if fo and f > b - fo:
        x = min(x, motion.ease((b - f) / float(fo)))
    return x


class Hud:
    def __init__(self, W, H):
        self.W, self.H, self.s = W, H, W / 1080.0
        self.rects = []                 # (name, (x0,y0,x1,y1)) of the current frame (for QA)
        self.cues = subtitles.build_cues()[0]
        g = np.zeros((H, W, 4), np.uint8)
        hh = int(H * 0.19)
        g[:hh, :, 3] = (150 * (1 - np.linspace(0, 1, hh)) ** 1.6).astype(np.uint8)[:, None]
        self.scrim = Image.fromarray(g, "RGBA")

    # ------------------------------------------------------------ primitives
    def px(self, v):
        return int(round(v * self.s))

    def _blit(self, cv, img, cx, cy, alpha=1.0, name=None, anchor="c"):
        if alpha <= 0.003 or img is None:
            return
        w, h = img.size
        x = int(cx - w / 2) if anchor == "c" else int(cx)
        y = int(cy - h / 2) if anchor == "c" else int(cy)
        if alpha < 0.999:
            a = img.getchannel("A").point(lambda v: int(v * alpha))
            img = img.copy()
            img.putalpha(a)
        x0, y0, x1, y1 = max(x, 0), max(y, 0), min(x + w, cv.width), min(y + h, cv.height)
        if x1 <= x0 or y1 <= y0:
            return
        sub = cv.crop((x0, y0, x1, y1))
        sub = Image.alpha_composite(sub, img.crop((x0 - x, y0 - y, x1 - x, y1 - y)))
        cv.paste(sub, (x0, y0))
        if name:
            self.rects.append((name, (x0, y0, x1, y1)))

    @lru_cache(maxsize=512)
    def _text(self, text, weight, size, color, shadow=True, spacing=0):
        fnt = font(weight, self.px(size))
        pad = self.px(14)
        dummy = ImageDraw.Draw(Image.new("L", (4, 4)))
        l, t, r, b = dummy.textbbox((0, 0), text, font=fnt)
        w, h = r - l + 2 * pad, b - t + 2 * pad
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        if shadow:
            sh = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            ImageDraw.Draw(sh).text((pad - l + self.px(2), pad - t + self.px(4)), text, font=fnt, fill=(0, 0, 0, 200))
            img = Image.alpha_composite(img, sh.filter(ImageFilter.GaussianBlur(self.px(5))))
        ImageDraw.Draw(img).text((pad - l, pad - t), text, font=fnt, fill=(*color, 255))
        return img

    def text(self, cv, s, x, y, size=48, color=WHITE, weight="Bold", alpha=1.0, name=None, shadow=True, anchor="c"):
        self._blit(cv, self._text(s, weight, size, tuple(color), shadow), self.px(x), self.px(y), alpha, name, anchor)

    @lru_cache(maxsize=2048)
    def _math(self, tex, size, color):
        buf = io.BytesIO()
        mathtext.math_to_image(tex, buf, prop=FontProperties(size=size * self.s), dpi=72, format="png",
                               color="black")
        arr = np.asarray(Image.open(buf).convert("L")).astype(float)
        out = np.zeros(arr.shape + (4,), np.uint8)
        out[:, :, :3] = color
        out[:, :, 3] = ((255 - arr)).astype(np.uint8)       # black-on-white -> alpha, then tint
        im = Image.fromarray(out, "RGBA")
        bb = im.getchannel("A").getbbox()
        return im.crop(bb) if bb else im

    def math(self, cv, tex, x, y, size=64, color=WHITE, alpha=1.0, name=None):
        self._blit(cv, self._math(tex, size, tuple(color)), self.px(x), self.px(y), alpha, name)

    @lru_cache(maxsize=64)
    def _panel(self, w, h, r, fill_a, accent):
        W, H = self.px(w), self.px(h)
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((0, 0, W - 1, H - 1), self.px(r), fill=(*PANEL, fill_a),
                            outline=(255, 255, 255, 38), width=max(1, self.px(2)))
        if accent:
            d.rounded_rectangle((0, self.px(r), self.px(8), H - self.px(r)), self.px(4), fill=(*accent, 255))
        return img

    def panel(self, cv, x, y, w, h, alpha=1.0, r=34, fill_a=190, accent=None, name=None):
        self._blit(cv, self._panel(w, h, r, fill_a, accent), self.px(x), self.px(y), alpha, name, "tl")

    @lru_cache(maxsize=64)
    def _bracket(self, h, side, color, thick):
        W, H = self.px(34), self.px(h)
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d, t = ImageDraw.Draw(img), self.px(thick)
        x = t // 2 if side == "L" else W - t // 2 - 1
        d.line((x, 0, x, H), fill=(*color, 255), width=t)
        x2 = W - 1 if side == "L" else 0
        d.line((x, t // 2, x2, t // 2), fill=(*color, 255), width=t)
        d.line((x, H - t // 2 - 1, x2, H - t // 2 - 1), fill=(*color, 255), width=t)
        return img

    def matrix(self, cv, cx, cy, entries, colors, cell_w, row_h, size, row_alpha, alpha=1.0, name=None,
               bracket_color=WHITE):
        """entries: 3x3 tex strings, colors: 3x3 rgb, row_alpha: per-row visibility (progressive reveal)."""
        h = 3 * row_h + 20
        bw = 3 * cell_w
        self._blit(cv, self._bracket(h, "L", bracket_color, 5), self.px(cx - bw / 2 - 22), self.px(cy), alpha * max(row_alpha[0], 0.0))
        self._blit(cv, self._bracket(h, "R", bracket_color, 5), self.px(cx + bw / 2 + 22), self.px(cy), alpha * max(row_alpha[0], 0.0))
        for i in range(3):
            for j in range(3):
                self.math(cv, entries[i][j], cx - bw / 2 + cell_w * (j + .5), cy - 1.5 * row_h + row_h * (i + .5) + 4,
                          size, colors[i][j], alpha * row_alpha[i])
        if name:
            self.rects.append((name, (self.px(cx - bw / 2 - 40), self.px(cy - h / 2), self.px(cx + bw / 2 + 40), self.px(cy + h / 2))))

    # ------------------------------------------------------------ scenes
    def render(self, f):
        self.rects = []
        cv = self.scrim.copy()
        if f < S2:
            self.scene1(cv, f)
        elif f < S3:
            self.scene2(cv, f)
        elif f < S4:
            self.scene3(cv, f)
        elif f < S5:
            self.scene4(cv, f)
        else:
            self.scene5(cv, f)
        self.caption(cv, f)
        return cv

    # -- scene 1 : kinetic typography
    def scene1(self, cv, f):
        lines = [["HOW", "DOES", "A", "ROBOT"], ["KNOW", "WHICH", "WAY"], ["TO", "TURN?"]]
        a_out = fade(f, 0, S2, 0, 14)
        k, size = 0, 84
        for li, words in enumerate(lines):
            widths = [self._text(w, "Bold", size, WHITE).width for w in words]
            gap = self.px(10)
            total = sum(widths) + gap * (len(words) - 1)
            x = self.W / 2 - total / 2
            y = 150 + li * 112
            for w, wd in zip(words, widths):
                t0 = 4 + 7 * k
                u = motion.ease((f - t0) / 14.0)
                col = ORANGE if w == "TURN?" else WHITE
                self._blit(cv, self._text(w, "Bold", size, col), x + wd / 2, self.px(y) + (1 - u) * self.px(46),
                           u * a_out, "title1")
                x += wd + gap
                k += 1

    # -- scene 2 : angle labels
    def scene2(self, cv, f):
        a = fade(f, S2 + 6, S3, 14, 12)
        self.text(cv, "THREE AXES · THREE ANGLES", 540, 118, 40, GREY, "SemiBold",
                  fade(f, S2 + 6, 225, 10, 6), "label")
        for ax, st, pk in motion.S2_DEMOS:
            al = fade(f, st - 2, st + 125, 10, 12)
            if al <= 0:
                continue
            col = {"X": RED, "Y": GREEN, "Z": BLUE}[ax]
            self.panel(cv, 90, 80, 900, 190, al, 40, 200, col)
            self.text(cv, f"ROTATION ABOUT {ax}", 540, 148, 66, WHITE, "Bold", al, "label")
            ang = motion.demo_angle(f, st, pk)
            self.math(cv, r"$\theta_%s=%d^\circ$" % (ax.lower(), round(ang)), 540, 226, 54, col, al)
        # explanation panel
        pa = fade(f, S2 + 20, S3, 16, 14)
        self.panel(cv, 90, 1176, 900, 400, pa, 36, 190, None, "panel")
        self.text(cv, "RIGHT-HAND RULE · +θ = COUNTER-CLOCKWISE", 540, 1228, 36, WHITE, "SemiBold", pa, "label")
        self.text(cv, "(looking from the +axis toward the origin)", 540, 1272, 30, GREY, "Medium", pa, "label")
        self.text(cv, "thin = fixed world axes   ·   thick = rotating tool axes", 540, 1352, 31, GREY, "Medium", pa, "label")
        self.text(cv, "ONE AXIS AT A TIME — NOT COMBINED", 540, 1425, 38, ORANGE, "Bold", pa, "label")
        self.text(cv, "combined rotations need a defined sequence", 540, 1486, 30, GREY, "Medium", pa, "label")

    # -- scene 3 : the matrix
    def scene3(self, cv, f):
        th = motion.theta_z_deg(f)
        self.text(cv, "REPRESENTING ROTATION", 540, 120, 46, WHITE, "Bold", fade(f, S3 + 8, 700, 14, 10), "label")
        pa = fade(f, 690, S4, 16, 14)
        self.panel(cv, 60, 1136, 960, 560, pa, 40, 200, ORANGE, "panel")
        rows = [fade(f, 705 + 24 * i, S4, 18, 0) for i in range(3)]
        hl = motion.ease((f - 800) / 30.0)
        o, w = tuple(int(WHITE[k] + (ORANGE[k] - WHITE[k]) * hl) for k in range(3)), (225, 228, 235)
        ent = [[r"$\cos\theta$", r"$-\sin\theta$", "$0$"], [r"$\sin\theta$", r"$\cos\theta$", "$0$"], ["$0$", "$0$", "$1$"]]
        col = [[o, o, w], [o, o, w], [w, w, w]]
        self.math(cv, r"$R_z(\theta)\;=$", 225, 1305, 62, WHITE, pa * rows[0], "matrix")
        self.matrix(cv, 700, 1305, ent, col, 200, 92, 58, rows, pa, "matrix")
        # live numeric matrix
        na = fade(f, 930, S4, 20, 0)
        if na > 0 and th is not None:
            c, s = math.cos(math.radians(th)), math.sin(math.radians(th))
            num = [[c, -s, 0], [s, c, 0], [0, 0, 1]]
            ent2 = [[self.num(v) for v in r] for r in num]
            col2 = [[ORANGE if (i < 2 and j < 2) else GREY for j in range(3)] for i in range(3)]
            self.math(cv, r"$\theta=%d^\circ$" % round(th), 215, 1544, 52, ORANGE, pa * na, "matrix")
            self.matrix(cv, 700, 1568, ent2, col2, 170, 56, 36, [1, 1, 1], pa * na, "matrix", GREY)
            self.text(cv, "v′ = Rz(θ) · v  :  coordinates of v rotate", 540, 1665, 30, GREY, "Medium", pa * na, "label")
            self.text(cv, "TRANSFORMS VECTOR COORDINATES", 540, 196, 34, ORANGE, "SemiBold", fade(f, 930, S4, 20, 12), "label")

    @staticmethod
    def num(v):
        v = round(v, 3) + 0.0
        if v == 0:
            return "$0$"
        if v == 1:
            return "$1$"
        if v == -1:
            return "$-1$"
        return "$%.3f$" % v

    # -- scene 4 : the 90 degree example
    def scene4(self, cv, f):
        th = motion.theta_z_deg(f)
        ta = fade(f, S4 + 8, S5, 14, 10)
        self.text(cv, "ROTATE 90° ABOUT +Z", 540, 118, 56, WHITE, "Bold", ta, "label")
        self.math(cv, r"$\theta=%d^\circ$" % round(th), 540, 205, 84, ORANGE, ta, "label")
        # legend: original / rotated local x
        la = fade(f, 1170, S5, 14, 10)
        self.text(cv, "BEFORE:  local x  →  world +X", 540, 292, 36, GREY, "SemiBold", la * (1 - motion.ease((f - 1300) / 30.0) * 0.6), "label")
        ra = fade(f, 1330, S5, 14, 10)
        self.text(cv, "AFTER:  local x′  →  world +Y", 540, 340, 40, ORANGE, "Bold", ra, "label")
        pa = fade(f, 1190, S5, 16, 12)
        self.panel(cv, 60, 1136, 960, 560, pa, 40, 205, ORANGE, "panel")
        c, s = math.cos(math.radians(th)), math.sin(math.radians(th))
        num = [[c, -s, 0], [s, c, 0], [0, 0, 1]]
        done = f >= S4_T1_F
        ent = [[self.num(v) for v in r] for r in num]
        col = [[(ORANGE if j == 0 else (225, 228, 235)) if done else (ORANGE if (i < 2 and j < 2) else GREY) for j in range(3)] for i in range(3)]
        self.math(cv, r"$R_z(%d^\circ)\;=$" % round(th), 235, 1290, 54, WHITE, pa, "matrix")
        self.matrix(cv, 710, 1290, ent, col, 180, 84, 52, [1, 1, 1], pa, "matrix")
        ea = fade(f, 1390, S5, 22, 10)
        self.math(cv, r"$R_z(90^\circ)\times\,[1,0,0]^T=[0,1,0]^T$", 540, 1520, 56, WHITE, pa * ea, "matrix")
        self.text(cv, "x-axis column  →  new direction of local x", 540, 1590, 30, ORANGE, "SemiBold", pa * ea, "label")
        al = fade(f, 1500, S5, 16, 10)
        self.text(cv, "PART ALIGNED WITH FIXTURE  ✓", 540, 1650, 34, (120, 235, 150), "Bold", pa * al, "label")

    # -- scene 5 : applications + outro
    def scene5(self, cv, f):
        chips = [(S5, S5B, "PRECISION ASSEMBLY"), (S5B, S5C, "ROBOTIC WELDING"), (S5C, S5D, "PICK-AND-PLACE")]
        for a, b, label in chips:
            al = fade(f, a + 6, b - 4, 8, 6)
            if al > 0:
                self.panel(cv, 150, 90, 780, 130, al, 36, 200, ORANGE, "label")
                self.text(cv, label, 540, 140, 58, WHITE, "Bold", al, "label")
                self.text(cv, "tool orientation changes smoothly", 540, 192, 30, GREY, "Medium", al, "label")
        ca = fade(f, S5D + 10, TOTAL_FRAMES, 18, 10)
        if ca > 0:
            self.text(cv, "ANGLES DESCRIBE.", 540, 190, 84, WHITE, "Bold", ca, "label")
            self.text(cv, "MATRICES TRANSFORM.", 540, 300, 84, ORANGE, "Bold", ca, "label")
        cb = fade(f, S5D + 34, TOTAL_FRAMES, 14, 10)
        if cb > 0:
            self.text(cv, "FOLLOW FOR MORE ROBOTICS", 540, 1500, 46, WHITE, "SemiBold", cb, "label")

    # -- captions (never drawn over the matrix panel: panel zone ends at y=1696)
    def caption(self, cv, f):
        c = subtitles.caption_at(f, self.cues)
        if not c:
            return
        words, lines, cur = c.split(), [], ""
        for w in words:
            t = (cur + " " + w).strip()
            if self._text(t, "SemiBold", 48, WHITE, False).width - self.px(28) > self.px(900) and cur:
                lines.append(cur); cur = w
            else:
                cur = t
        lines.append(cur)
        y0 = 1782 - (len(lines) - 1) * 31
        ww = max(self._text(l, "SemiBold", 48, WHITE, False).width for l in lines) - self.px(28) + self.px(56)
        hh = self.px(62 * len(lines) + 24)
        pan = Image.new("RGBA", (ww, hh), (0, 0, 0, 0))
        ImageDraw.Draw(pan).rounded_rectangle((0, 0, ww - 1, hh - 1), self.px(30), fill=(0, 0, 0, 150))
        self._blit(cv, pan, self.W / 2, self.px(y0 + (len(lines) - 1) * 31), 1.0, "caption")
        for i, l in enumerate(lines):
            self.text(cv, l, 540, y0 + i * 62, 48, WHITE, "SemiBold", 1.0, None, False)


S4_T1_F = motion.S4_T1


def dip_factor(f):
    """Brightness multiplier of the 3-D layer for the fade-through-black transitions."""
    k = 1.0
    for b, hw in DIPS:
        d = abs(f - b)
        if d < hw:
            k = min(k, motion.ease(d / float(hw)))
    return k
