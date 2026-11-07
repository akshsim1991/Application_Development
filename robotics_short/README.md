# How Do Industrial Robots Know Which Way to Turn? — Blender YouTube Short (60 s, 9:16)

Fully scripted, free-tools-only pipeline (Blender `bpy` + Cycles, Pillow/matplotlib for the sharp 2-D layer, ffmpeg).
Works on Windows, macOS and Linux; all paths are project-relative.

## Install
```
pip install bpy numpy pillow matplotlib          # or install Blender (free) and set $BLENDER
# ffmpeg/ffprobe must be on PATH
python run.py check
```

## Run
```
python run.py math                       # numeric validation (documentation/math_validation.json)
python run.py build                      # scenes/robotics_short.blend
python run.py render --preset preview    # 540x960, Cycles low-sample + denoise, PNG sequence in renders/preview
python run.py composite --preset preview # + captions, titles, matrix typography -> renders/preview_composited
python run.py srt                        # subtitles/captions.srt
python run.py encode --preset preview    # exports/robotics_short_preview.mp4 (+ verification JSON)
python run.py qa --preset preview        # QA report + contact sheet in documentation/
python run.py all-preview                # everything above
python run.py render --preset final      # 2160x3840 Cycles + OIDN denoise, GPU if present else CPU (very slow on CPU!)
python run.py composite --preset final ; python run.py encode --preset final
```
Rendering is resumable (existing PNGs are skipped); use `--start/--end/--step/--frames/--shard i/n` to split work.
Eevee is not used because headless/GPU-less machines have no GL context; the PREVIEW preset is Cycles at 4 spp.

## Narration (Indian English, en-IN)
The script is locked in `blender_scripts/config.py` (`NARRATION`) and is never altered.
1. `python blender_scripts/tts_indic_parler.py --sample [--audition]` → ~10 s sample(s) in `audio/` using the free
   local model `ai4bharat/indic-parler-tts` (needs torch + parler-tts; model download from huggingface.co).
2. Approve the voice, then `--full --speaker NAME` → `audio/narration.wav` (48 kHz stereo).
3. `python blender_scripts/audio_sync.py audio/narration.wav` → measures speech blocks, writes `audio/segment_timing.json`,
   reports any timing conflict (never edits the script). Re-run `srt`, `composite`, `encode`.
Without `audio/narration.wav` the video carries a silent 48 kHz stereo track and nominal-timed caption placeholders.

## Layout
`blender_scripts/` config, motion (single source of truth for every angle), kinematics (6R FK/IK), bl_* (robot, environment,
materials, lighting, camera, axes/arcs), build_scene, render_setup, render_frames, hud (overlay + matrix), subtitles,
composite, encode, qa, qa_scene, mathcheck, audio_sync, tts_indic_parler. `assets/fonts` (Inter, OFL), `scenes/`,
`renders/`, `audio/`, `subtitles/`, `exports/`, `documentation/`.

## Conventions
Right-handed axes, column vectors, active rotations. Rz(θ)=[[c,−s,0],[s,c,0],[0,0,1]]. Fixed world axes are thin; rotating tool
axes (x′ y′ z′) are thick. Scene 2 shows single-axis rotations only; the sequence caveat for combined rotations is stated on screen.
