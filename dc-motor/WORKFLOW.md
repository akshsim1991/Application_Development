# DC Motor Animation + Narration — Workflow

Goal: a ~2–3 min explainer video showing how a brushed DC motor works, with synced voice-over.

## Environment check (done)
| Need | Status |
|---|---|
| Node 22 + Playwright/Chromium (render frames) | available |
| ffmpeg 6.1 (encode, mux audio) | available |
| Python 3.13 + numpy + pillow | available |
| Text-to-speech | **none installed** — pick one in Stage 3 |

## Pipeline

```
1 script  ->  2 storyboard/timeline  ->  3 narration audio  ->  4 animation  ->  5 render  ->  6 mux + QA
```

### Stage 1 — Script (`script/narration.md`)
Write the narration as numbered scenes, one short paragraph each (~15–25 s). Draft outline:
1. **Intro** – a DC motor turns electrical energy into rotation.
2. **Parts** – stator magnets (N/S), rotor/armature coil, commutator, brushes, battery.
3. **Magnetic field** – field lines from N to S across the gap.
4. **Force on a conductor** – current in a field gives force; Fleming's left-hand rule, F = B·I·L.
5. **Torque** – opposite forces on the two coil sides form a couple; the coil rotates.
6. **Dead spot** – at the vertical position torque goes to zero.
7. **Commutator** – split ring reverses current each half turn, so torque keeps one direction.
8. **Back EMF & speed** – spinning coil generates opposing voltage; speed settles.
9. **Recap** – more current/field/turns means more torque; applications.

### Stage 2 — Timeline (`script/timeline.json`)
One entry per scene: `{ id, text, start, end, visuals: [...] }`.
Start with estimated durations (~150 words/min); Stage 3 overwrites them with the real audio lengths so animation and audio cannot drift.

### Stage 3 — Narration (`narration/`)
- One audio file per scene (`narration/s01.wav` …) so scenes can be re-recorded individually.
- `narration/make_audio.py` reads `timeline.json`, calls the TTS backend, measures each file's duration with `ffprobe`, and writes the durations back to the timeline.
- Backend options (decision needed — see below): Piper (offline, free), edge-tts (online, natural voices), or a commercial API (ElevenLabs/OpenAI), or your own recording.

### Stage 4 — Animation (`animation/`)
Single-page HTML + SVG/Canvas, no heavy dependencies:
- `index.html` renders the motor (front view of the coil, magnets, commutator, brushes, battery).
- Everything is a pure function of time: `render(t)`. No `setTimeout` or free-running loops, so frame capture is deterministic.
- Scene visibility, highlights, current arrows (red/blue), force arrows, and field lines are driven by `timeline.json`.
- Rotor angle θ(t) follows the physics: torque ∝ sin-term, commutator flips current at θ = 90° / 270°.
- Captions (subtitles) are drawn from the same timeline.
- A live preview mode (play/pause/scrub) is included for iterating without rendering.

### Stage 5 — Render (`build/`)
- `build/render_frames.js`: Playwright opens `index.html?t=<seconds>` for each frame at 30 fps, 1920×1080, saves PNGs to `build/frames/`.
- `ffmpeg -framerate 30 -i frames/%05d.png -c:v libx264 -pix_fmt yuv420p silent.mp4`

### Stage 6 — Mux + QA (`output/`)
- Concatenate scene audio with silences matching the timeline into `narration.wav`.
- `ffmpeg -i silent.mp4 -i narration.wav -c:v copy -c:a aac -shortest output/dc_motor.mp4`
- Optional: export `.srt`/`.vtt` captions from the timeline.
- QA checklist: each visual cue lands on the matching words; arrow directions are physically correct (left-hand rule); the commutator flip happens at the right angle; audio levels are consistent; the video ends cleanly.

## Deliverables
- `output/dc_motor.mp4` (final video) and `output/dc_motor.srt`
- `animation/index.html` (interactive version, usable standalone)

## Open decisions for you
1. **Voice**: offline (Piper, robotic-ish but free), online TTS (edge-tts, good quality), a paid API, or your own recording?
2. **Level**: school-level (concepts, left-hand rule) or engineering-level (equations, back EMF, torque curves)?
3. **Language/length**: English, ~2–3 min OK?
4. **Format**: MP4 video, interactive HTML page, or both?

## Build order
1. Approve outline + answer the questions above
2. Script + timeline  →  3. Audio  →  4. Animation preview  →  5. Render  →  6. Mux/QA
