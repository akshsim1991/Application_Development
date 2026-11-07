"""Composite: 3-D frame (dipped to black at transitions) + HUD overlay + captions.

    python blender_scripts/composite.py --preset preview [--start 0 --end 1799] [--contact]
"""
import os
import sys
import argparse
import time
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import *    # noqa
import hud as H


def available(dirpath):
    fs = {}
    for n in os.listdir(dirpath):
        if n.startswith("frame_") and n.endswith(".png") and ".part" not in n:
            fs[int(n[6:10])] = os.path.join(dirpath, n)
    return fs


def run(preset, start=0, end=TOTAL_FRAMES - 1, size=None, src=None, dst=None, overwrite=False):
    p = PRESETS[preset]
    src = src or path("renders", preset)
    dst = dst or path("renders", preset + "_composited")
    os.makedirs(dst, exist_ok=True)
    have = available(src)
    if not have:
        raise SystemExit(f"no rendered frames in {src}")
    keys = sorted(have)
    W, Hh = size or (p["w"], p["h"])
    hud = H.Hud(W, Hh)
    t0, n, last_img, last_key = time.time(), 0, None, None
    for f in range(start, end + 1):
        out = os.path.join(dst, f"frame_{f:04d}.png")
        if os.path.exists(out) and not overwrite:
            continue
        # nearest rendered frame at or before f (supports stepped/partial preview renders)
        k = max([x for x in keys if x <= f], default=keys[0])
        if k != last_key:
            last_img, last_key = Image.open(have[k]).convert("RGB"), k
        base = last_img
        if base.size != (W, Hh):
            base = base.resize((W, Hh), Image.LANCZOS)
        d = H.dip_factor(f)
        if d < 0.999:
            base = Image.eval(base, lambda v: int(v * d))
        img = base.convert("RGBA")
        img = Image.alpha_composite(img, hud.render(f))
        img.convert("RGB").save(out + ".tmp.png", compress_level=3)
        os.replace(out + ".tmp.png", out)
        n += 1
        if n % 100 == 0:
            print(f"[composite] {f} ({n} done, {(time.time()-t0)/n:.2f}s/frame)", flush=True)
    return dst, n


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="preview", choices=list(PRESETS))
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--end", type=int, default=TOTAL_FRAMES - 1)
    ap.add_argument("--overwrite", action="store_true")
    ap.add_argument("--delivery", action="store_true", help="resize to 1080x1920 delivery size")
    a = ap.parse_args()
    dst, n = run(a.preset, a.start, a.end, DELIVERY if a.delivery else None,
                 dst=path("renders", a.preset + ("_delivery" if a.delivery else "_composited")),
                 overwrite=a.overwrite)
    print("composited", n, "frames ->", dst)
