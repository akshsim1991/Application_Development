"""Measure a supplied narration file and report/derive caption timing.

    python blender_scripts/audio_sync.py audio/narration.wav

Detects the 5 speech blocks via ffmpeg silencedetect (blocks are separated by the
longest pauses), writes audio/segment_timing.json (used by subtitles.py) and
prints a timing report.  The script text is never modified: if a block is longer
than its window, the conflict is reported instead.
"""
import json
import os
import re
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import *    # noqa


def duration(p):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", p],
                         capture_output=True, text=True).stdout
    return float(out.strip())


def main(p):
    dur = duration(p)
    r = subprocess.run(["ffmpeg", "-hide_banner", "-i", p, "-af", "silencedetect=noise=-38dB:d=0.25", "-f", "null", "-"],
                       capture_output=True, text=True).stderr
    starts = [float(x) for x in re.findall(r"silence_start: ([\d.]+)", r)]
    ends = [float(x) for x in re.findall(r"silence_end: ([\d.]+)", r)]
    gaps = sorted(zip(starts, ends[:len(starts)]), key=lambda g: g[0])
    inner = [g for g in gaps if g[0] > 0.3 and g[1] < dur - 0.1]
    cuts = sorted(sorted(inner, key=lambda g: g[1] - g[0], reverse=True)[:len(NARRATION) - 1])
    lead = next((e for s, e in gaps if s < 0.05), 0.0)
    bounds, t = [], lead
    for s, e in cuts:
        bounds.append((t, s)); t = e
    bounds.append((t, dur))
    segs, ok = [], True
    for (a, b), (na, nb, text) in zip(bounds, NARRATION):
        fits = (b - a) <= (nb - na) + 1e-6
        ok &= fits
        # place each block at the start of its allocated window
        segs.append(dict(start=na, end=na + (b - a), measured=round(b - a, 3), window=nb - na, fits=fits))
    json.dump(dict(source=os.path.basename(p), audio_duration=dur, segments=segs),
              open(path("audio", "segment_timing.json"), "w"), indent=2)
    for i, s in enumerate(segs, 1):
        print(f"block {i}: speech {s['measured']:.2f}s / window {s['window']:.0f}s  {'OK' if s['fits'] else 'TIMING CONFLICT'}")
    print("audio total %.2fs (video 60.00s)" % dur)
    if not ok:
        print("CONFLICT: narration longer than its allocated window; script left unchanged - re-record faster or adjust windows.")
    return ok


if __name__ == "__main__":
    sys.exit(0 if main(sys.argv[1]) else 2)
