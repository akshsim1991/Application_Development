"""Render a PNG sequence (resumable: existing frames are skipped).

    python blender_scripts/render_frames.py --preset preview --start 0 --end 1799
    python blender_scripts/render_frames.py --preset preview --frames 0,100,700
    blender -b scenes/robotics_short.blend -P blender_scripts/render_frames.py -- --preset final
"""
import os
import sys
import time
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from config import *    # noqa
import render_setup


def parse():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="preview", choices=list(PRESETS))
    ap.add_argument("--blend", default=path("scenes", "robotics_short.blend"))
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--end", type=int, default=TOTAL_FRAMES - 1)
    ap.add_argument("--step", type=int, default=1)
    ap.add_argument("--frames", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--samples", type=int, default=0)
    ap.add_argument("--shard", default="0/1", help="i/n : render every n-th frame starting at i")
    return ap.parse_args(argv)


def main():
    a = parse()
    if bpy.data.filepath == "":
        bpy.ops.wm.open_mainfile(filepath=a.blend)
    dev = render_setup.apply(a.preset)
    if a.samples:
        bpy.context.scene.cycles.samples = a.samples
    out = a.out or path("renders", a.preset)
    os.makedirs(out, exist_ok=True)
    frames = [int(x) for x in a.frames.split(",") if x] or list(range(a.start, a.end + 1, a.step))
    si, sn = [int(x) for x in a.shard.split("/")]
    frames = [f for k, f in enumerate(frames) if k % sn == si]
    sc = bpy.context.scene
    print(f"[render] preset={a.preset} device={dev} size={sc.render.resolution_x}x{sc.render.resolution_y} "
          f"samples={sc.cycles.samples} frames={len(frames)} -> {out}", flush=True)
    t0 = time.time(); done = 0
    for f in frames:
        fp = os.path.join(out, f"frame_{f:04d}.png")
        if os.path.exists(fp) and os.path.getsize(fp) > 0:
            continue
        sc.frame_set(f)
        sc.render.filepath = fp + ".part"
        bpy.ops.render.render(write_still=True)
        os.replace(fp + ".part", fp) if os.path.exists(fp + ".part") else os.replace(fp + ".part.png", fp)
        done += 1
        if done % 10 == 0 or len(frames) < 20:
            el = time.time() - t0
            print(f"[render] frame {f} done={done} {el/done:.1f}s/frame", flush=True)
    print(f"[render] finished {done} new frames in {time.time()-t0:.0f}s", flush=True)


if __name__ == "__main__":
    main()
