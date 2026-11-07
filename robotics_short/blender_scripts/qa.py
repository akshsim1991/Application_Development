"""Non-bpy QA: math, subtitles, text sizes, lighting continuity, frame count, contact sheet.

    python blender_scripts/qa.py --preset preview
"""
import argparse
import json
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image
from config import *    # noqa
import mathcheck, subtitles, hud as H, motion


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="preview")
    a = ap.parse_args()
    R, rep = [], {}

    def add(name, ok, detail=""):
        R.append(dict(check=name, status="PASS" if ok else "FAIL", detail=detail))

    ok, res = mathcheck.run()
    add("1-3. Rotation matrices, positive directions, 90-degree transform", ok, f"{len(res)} numeric checks")

    sj = path("documentation", "qa_scene.json")
    if os.path.exists(sj):
        q = json.load(open(sj))
        add("4. Mesh intersections (BVH, sampled frames)", not q["intersections"],
            f"{len(q['intersections'])} overlapping pairs: " + "; ".join(f"{i['a']}/{i['b']}@{i['frame']}" for i in q["intersections"][:8]))
        add("Animation == kinematics (tool_frame matrix_world)", q["pose_error"] < 1e-4, f"max err {q['pose_error']:.2e}")
        bad = [x for x in q["framing"] if not x["in_front"] or not (0.12 < x["x"] < 0.88)
               or not (0.05 < x["y_from_top"] < 0.62 if S2 + 15 <= x["frame"] < S5 else 0.05 < x["y_from_top"] < 0.88)]
        add("5. Camera framing: TCP inside frame and clear of panel/caption zones", not bad,
            f"{len(q['framing'])} frames, outliers: " + ", ".join(f"{b['frame']}(x={b['x']},y={b['y_from_top']})" for b in bad[:10]))
    else:
        add("4-5. Intersections/framing", False, "run qa_scene.py first")

    # 6 text readability: min on-screen size & panel/caption overlap
    sizes, overlaps = [], []
    hud = H.Hud(540, 960)
    orig = hud.text
    def rec(cv, s, x, y, size=48, *aa, **kw):
        sizes.append(size); return orig(cv, s, x, y, size, *aa, **kw)
    hud.text = rec
    for f in range(0, TOTAL_FRAMES, 10):
        hud.render(f)
        cap = [r for n, r in hud.rects if n == "caption"]
        pan = [r for n, r in hud.rects if n in ("panel", "matrix")]
        for c in cap:
            for p in pan:
                if not (c[2] < p[0] or c[0] > p[2] or c[3] < p[1] or c[1] > p[3]):
                    overlaps.append((f, "panel/matrix"))
    add("6. Text readability (min size at 1080x1920 >= 30 px)", min(sizes) >= 30, f"min text size {min(sizes)} px")
    add("Captions never overlap matrix/panels", not overlaps, f"{len(overlaps)} overlaps")

    # 7 subtitles
    cues, src = subtitles.build_cues()
    joined = " ".join(c for _, _, c in cues)
    verbatim = joined == " ".join(t for _, _, t in NARRATION)
    mono = all(cues[i][1] <= cues[i + 1][0] + 1e-9 for i in range(len(cues) - 1))
    inwin = all(any(a - 1e-6 <= s and e <= b + 1e-6 for a, b, _ in NARRATION) for s, e, _ in cues)
    add("7. Subtitles verbatim, ordered, inside script windows", verbatim and mono and inwin,
        f"{len(cues)} cues, timing source: {src}")
    rep["timing"] = subtitles.timing_report()
    add("Narration pace (<=3.0 words/s per block)", all(r["status"] == "OK" for r in rep["timing"]),
        ", ".join(f"{r['window']}:{r['wps']}w/s" for r in rep["timing"]))

    # 8 lighting consistency (3-D layer mean luminance, adjacent sampled frames)
    d3 = path("renders", a.preset)
    if os.path.isdir(d3):
        fs = sorted(int(n[6:10]) for n in os.listdir(d3) if n.endswith(".png") and ".part" not in n)
        lum = {}
        for f in fs[::6]:
            lum[f] = float(np.asarray(Image.open(os.path.join(d3, f"frame_{f:04d}.png")).convert("L").resize((54, 96))).mean())
        ks = sorted(lum)
        jumps = [(ks[i], round(lum[ks[i]], 1), round(lum[ks[i + 1]], 1)) for i in range(len(ks) - 1)
                 if abs(lum[ks[i + 1]] - lum[ks[i]]) > 0.35 * max(lum[ks[i]], 1)
                 and not any(abs(ks[i] - b) <= 12 for b, _ in DIPS)]
        add("8. Lighting consistency (no luminance jumps >35% outside transitions)", not jumps,
            f"{len(ks)} samples, mean luminance {np.mean(list(lum.values())):.0f}/255, jumps: {jumps[:6]}")
        add("Rendered 3-D frame count", len(fs) == TOTAL_FRAMES, f"{len(fs)}/{TOTAL_FRAMES}")
    # 9-10 output
    dc = path("renders", a.preset + "_composited")
    n = len([x for x in os.listdir(dc)]) if os.path.isdir(dc) else 0
    add("9. Composited frame count == 1800", n == TOTAL_FRAMES, f"{n}")
    ej = path("documentation", f"export_{a.preset}.json")
    if os.path.exists(ej):
        e = json.load(open(ej))
        add("9. MP4 frame count / duration", e["frames"] == TOTAL_FRAMES and abs(e["duration"] - 60) < 0.05,
            f"{e['frames']} frames, {e['duration']:.3f}s, {e['width']}x{e['height']} {e['codec']} {e['pix_fmt']}")
        add("10. Export integrity (full decode, audio track)", not e["decode_errors"] and bool(e["audio"]),
            f"audio={e['audio']} ({e['narration']})")
        # contact sheet
        if n == TOTAL_FRAMES:
            fr = list(range(0, TOTAL_FRAMES, 100)) + [1799]
            ims = [Image.open(os.path.join(dc, f"frame_{f:04d}.png")).resize((270, 480)) for f in fr]
            cols = 7
            rows = -(-len(ims) // cols)
            sh = Image.new("RGB", (270 * cols, 480 * rows))
            for i, im in enumerate(ims):
                sh.paste(im, ((i % cols) * 270, (i // cols) * 480))
            sp = path("documentation", f"contact_sheet_{a.preset}.png")
            sh.save(sp)
            rep["contact_sheet"] = sp
    rep["checks"] = R
    json.dump(rep, open(path("documentation", f"qa_report_{a.preset}.json"), "w"), indent=1)
    for r in R:
        print(f"{r['status']}  {r['check']}  — {r['detail']}")


if __name__ == "__main__":
    main()
