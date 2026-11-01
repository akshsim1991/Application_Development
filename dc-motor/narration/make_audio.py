"""Synthesize narration with Kokoro and write narration.wav + animation/timeline.js."""
import json, os, sys
import numpy as np, soundfile as sf
from kokoro_onnx import Kokoro

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KDIR = os.environ.get("KOKORO_DIR", "/tmp/kokoro")
LEAD, CUE_GAP, SCENE_GAP, OUTRO = 1.5, 0.35, 1.0, 4.5

spec = json.load(open(f"{ROOT}/script/scenes.json"))
k = Kokoro(f"{KDIR}/kokoro-v1.0.onnx", f"{KDIR}/voices-v1.0.bin")
sr = 24000
chunks, t = [np.zeros(int(LEAD * sr), dtype=np.float32)], LEAD
scenes = []
for si, sc in enumerate(spec["scenes"]):
    cues = []
    for ci, cue in enumerate(sc["cues"]):
        a, _ = k.create(cue.get("say", cue["text"]), voice=spec["voice"], speed=spec["speed"], lang="en-us")
        a = a.astype(np.float32)
        cues.append({"text": cue["text"], "start": round(t, 3), "end": round(t + len(a) / sr, 3)})
        chunks.append(a); t += len(a) / sr
        last = ci == len(sc["cues"]) - 1
        gap = SCENE_GAP if last else CUE_GAP
        chunks.append(np.zeros(int(gap * sr), dtype=np.float32)); t += gap
    scenes.append({"title": sc["title"], "cues": cues})
narr_end = scenes[-1]["cues"][-1]["end"]
total = round(narr_end + OUTRO, 3)
for i, sc in enumerate(scenes):
    sc["start"] = 0.0 if i == 0 else round(sc["cues"][0]["start"] - SCENE_GAP / 2, 3)
for i, sc in enumerate(scenes):
    sc["end"] = scenes[i + 1]["start"] if i + 1 < len(scenes) else total
audio = np.concatenate(chunks)
audio = np.concatenate([audio, np.zeros(max(0, int(total * sr) - len(audio)), dtype=np.float32)])
audio *= 0.89 / max(1e-6, np.abs(audio).max())
os.makedirs(f"{ROOT}/build", exist_ok=True)
sf.write(f"{ROOT}/build/narration.wav", audio, sr)
json.dump({"total": total, "scenes": scenes}, open(f"{ROOT}/script/timeline.json", "w"), indent=1)
open(f"{ROOT}/animation/timeline.js", "w").write("window.TIMELINE=" + json.dumps({"total": total, "scenes": scenes}) + ";")
# SRT captions
def ts(x): h=int(x//3600); m=int(x%3600//60); s=x%60; return f"{h:02d}:{m:02d}:{s:06.3f}".replace(".",",")
n=0; out=[]
for sc in scenes:
    for c in sc["cues"]:
        n+=1; out.append(f"{n}\n{ts(c['start'])} --> {ts(c['end'])}\n{c['text']}\n")
os.makedirs(f"{ROOT}/output", exist_ok=True)
open(f"{ROOT}/output/dc_motor.srt","w").write("\n".join(out))
print("total seconds:", total, " narration ends:", narr_end)
for i, sc in enumerate(scenes): print(i, sc["start"], sc["end"], sc["title"])
