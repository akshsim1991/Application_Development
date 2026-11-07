#!/usr/bin/env python3
"""Pipeline driver (Windows + macOS + Linux).  Project-relative paths only.

  python run.py check                 # environment report (Blender/bpy, ffmpeg, GPU, fonts, TTS deps)
  python run.py math                  # numeric validation of all rotation claims
  python run.py build                 # build scenes/robotics_short.blend
  python run.py render --preset preview|final [--frames 0,100 | --start N --end M]
  python run.py composite --preset preview|final
  python run.py srt
  python run.py encode --preset preview|final
  python run.py qa --preset preview|final
  python run.py all-preview           # build + render + composite + srt + encode + qa

Blender is located via $BLENDER, PATH, or default install folders; otherwise the pip
package `bpy` (pip install bpy numpy pillow matplotlib) in the current interpreter is used.
"""
import glob
import importlib.util
import os
import platform
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
S = os.path.join(ROOT, "blender_scripts")


def find_blender():
    cands = [os.environ.get("BLENDER"), shutil.which("blender")]
    if platform.system() == "Darwin":
        cands.append("/Applications/Blender.app/Contents/MacOS/Blender")
    if platform.system() == "Windows":
        cands += sorted(glob.glob(r"C:\Program Files\Blender Foundation\Blender *\blender.exe"), reverse=True)
    return next((c for c in cands if c and os.path.exists(c)), None)


def have(mod):
    return importlib.util.find_spec(mod) is not None


def bpy_script(script, args):
    """Run a bpy script via Blender binary if found, else via the bpy module."""
    exe = find_blender()
    p = os.path.join(S, script)
    if exe:
        return subprocess.call([exe, "-b", "-P", p, "--"] + args)
    if not have("bpy"):
        sys.exit("Blender not found and python module 'bpy' missing. Install Blender (free) or: pip install bpy")
    return subprocess.call([sys.executable, p] + args)


def py(script, args):
    return subprocess.call([sys.executable, os.path.join(S, script)] + args)


def check():
    print("python      :", sys.version.split()[0], platform.platform())
    exe = find_blender()
    print("blender bin :", exe or "not found")
    if have("bpy"):
        import bpy
        print("bpy module  :", bpy.app.version_string)
    for m in ("numpy", "PIL", "matplotlib"):
        print(f"{m:12s}:", "ok" if have(m) else "MISSING (pip install numpy pillow matplotlib)")
    for t in ("ffmpeg", "ffprobe"):
        print(f"{t:12s}:", shutil.which(t) or "MISSING")
    print("fonts       :", "ok" if os.path.exists(os.path.join(ROOT, "assets/fonts/Inter-Bold.otf")) else "MISSING")
    print("GPU         :", "NVIDIA " + subprocess.getoutput("nvidia-smi -L").split("\n")[0]
          if shutil.which("nvidia-smi") else "none detected (Cycles falls back to CPU; Apple Silicon uses Metal automatically)")
    print("TTS deps    :", "torch+parler_tts ok" if have("torch") and have("parler_tts") else "not installed (see tts_indic_parler.py)")
    print("cpu cores   :", os.cpu_count())


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, rest = sys.argv[1], sys.argv[2:]
    if cmd == "check":
        check()
    elif cmd == "math":
        sys.exit(py("mathcheck.py", rest))
    elif cmd == "build":
        sys.exit(bpy_script("build_scene.py", rest))
    elif cmd == "render":
        sys.exit(bpy_script("render_frames.py", rest))
    elif cmd == "composite":
        sys.exit(py("composite.py", rest))
    elif cmd == "srt":
        sys.exit(py("subtitles.py", rest))
    elif cmd == "encode":
        sys.exit(py("encode.py", rest))
    elif cmd == "qa":
        bpy_script("qa_scene.py", [])
        sys.exit(py("qa.py", rest))
    elif cmd == "all-preview":
        for step in (["math"], ["build"], ["render", "--preset", "preview"], ["composite", "--preset", "preview"],
                     ["srt"], ["encode", "--preset", "preview"], ["qa", "--preset", "preview"]):
            sys.argv = [sys.argv[0]] + step
            print("==>", " ".join(step), flush=True)
            main_step = subprocess.call([sys.executable, __file__] + step)
            if main_step:
                sys.exit(main_step)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
