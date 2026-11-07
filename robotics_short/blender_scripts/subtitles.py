"""Caption chunking + SRT export. Text comes ONLY from config.NARRATION (verbatim).

Timing source, in priority order:
  1. audio/segment_timing.json  (measured from the real narration by audio_sync.py)
  2. nominal windows from the locked script (placeholders for a silent video)
"""
import json
import os
import re
from config import *    # noqa

MAX_WORDS = 9


def segment_windows():
    p = path("audio", "segment_timing.json")
    if os.path.exists(p):
        d = json.load(open(p))
        return [(s["start"], s["end"]) for s in d["segments"]], "measured-from-audio"
    return [(a, b) for a, b, _ in NARRATION], "nominal-placeholder"


def _sentences(text):
    return [s for s in re.split(r"(?<=[.?!])\s+", text.strip()) if s]


def _chunks(sentence):
    w = sentence.split()
    if len(w) <= MAX_WORDS + 1:
        return [sentence]
    n = -(-len(w) // MAX_WORDS)
    size = -(-len(w) // n)
    return [" ".join(w[i:i + size]) for i in range(0, len(w), size)]


def build_cues():
    wins, source = segment_windows()
    cues = []
    for (a, b), (_, _, text) in zip(wins, NARRATION):
        chunks = [c for s in _sentences(text) for c in _chunks(s)]
        weights = [len(c) + 4 for c in chunks]          # chars + small pause allowance
        tot, t = float(sum(weights)), a
        for c, w in zip(chunks, weights):
            d = (b - a) * w / tot
            cues.append((t, t + d, c))
            t += d
    return cues, source


def ts(t):
    ms = int(round(t * 1000))
    return f"{ms // 3600000:02d}:{ms // 60000 % 60:02d}:{ms // 1000 % 60:02d},{ms % 1000:03d}"


def write_srt(out=None):
    cues, source = build_cues()
    out = out or path("subtitles", "captions.srt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as fh:
        for i, (a, b, c) in enumerate(cues, 1):
            fh.write(f"{i}\n{ts(a)} --> {ts(b - 0.001)}\n{c}\n\n")
    return out, source, len(cues)


def caption_at(frame, cues=None):
    cues = cues or build_cues()[0]
    t = frame / FPS
    for a, b, c in cues:
        if a <= t < b:
            return c
    return None


def timing_report():
    """Words-per-second per narration block (flags blocks that are too fast to speak)."""
    rows = []
    for a, b, text in NARRATION:
        n = len(text.replace("-", " ").split())
        rows.append(dict(window=f"{a:.0f}-{b:.0f}s", words=n, seconds=b - a, wps=round(n / (b - a), 2),
                         status="OK" if n / (b - a) <= 3.0 else "TOO FAST"))
    return rows


if __name__ == "__main__":
    print(write_srt())
    for r in timing_report():
        print(r)
