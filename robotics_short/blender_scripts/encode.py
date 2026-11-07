"""Encode composited PNG sequence (+ narration or silent stereo track) to MP4 and verify it.

    python blender_scripts/encode.py --preset preview
"""
import argparse
import json
import os
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import *    # noqa


def probe(p):
    out = subprocess.run(["ffprobe", "-v", "error", "-count_frames", "-show_streams", "-show_format", "-of", "json", p],
                         capture_output=True, text=True).stdout
    d = json.loads(out)
    v = next(s for s in d["streams"] if s["codec_type"] == "video")
    au = [s for s in d["streams"] if s["codec_type"] == "audio"]
    return dict(codec=v["codec_name"], width=v["width"], height=v["height"], fps=v["r_frame_rate"],
                frames=int(v["nb_read_frames"]), duration=float(d["format"]["duration"]),
                pix_fmt=v["pix_fmt"], audio=[dict(codec=s["codec_name"], rate=s["sample_rate"], channels=s["channels"]) for s in au])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="preview")
    ap.add_argument("--src", default="")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    src = a.src or path("renders", a.preset + "_composited")
    n = len([x for x in os.listdir(src) if x.startswith("frame_") and x.endswith(".png")])
    if n != TOTAL_FRAMES:
        sys.exit(f"refusing to encode: {n} frames in {src}, expected {TOTAL_FRAMES}")
    out = a.out or path("exports", f"robotics_short_{a.preset}.mp4")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    nar = path("audio", "narration.wav")
    has_audio = os.path.exists(nar)
    cmd = ["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", os.path.join(src, "frame_%04d.png")]
    cmd += ["-i", nar] if has_audio else ["-f", "lavfi", "-i", f"anullsrc=r=48000:cl=stereo:d={DURATION_S}"]
    crf = "14" if a.preset == "final" else "20"
    cmd += ["-c:v", "libx264", "-preset", "slow" if a.preset == "final" else "medium", "-crf", crf,
            "-pix_fmt", "yuv420p", "-r", str(FPS), "-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2",
            "-t", str(DURATION_S), "-movflags", "+faststart", out]
    subprocess.run(cmd, check=True)
    info = probe(out)
    dec = subprocess.run(["ffmpeg", "-v", "error", "-i", out, "-f", "null", "-"], capture_output=True, text=True)
    info.update(decode_errors=dec.stderr.strip(), narration="real narration" if has_audio else "SILENT placeholder track",
                size_mb=round(os.path.getsize(out) / 1e6, 2), file=out)
    json.dump(info, open(path("documentation", f"export_{a.preset}.json"), "w"), indent=1)
    print(json.dumps(info, indent=1))


if __name__ == "__main__":
    main()
