import numpy as np, soundfile as sf, json, sys, os
from kokoro_onnx import Kokoro
from script_data import SCENES
SPEED = float(sys.argv[1]) if len(sys.argv) > 1 else 1.08
k = Kokoro("/tmp/models/kokoro-v1.0.onnx", "/tmp/models/voices-v1.0.bin")
SR = 24000
GAP_S = 0.28      # pause between sentences
GAP_SCENE = 0.9   # pause at end of scene
out = []; timing = []; t = 0.0
for si, (sid, title, sents) in enumerate(SCENES):
    s0 = t; caps = []
    for j, s in enumerate(sents):
        a, sr = k.create(s, voice="af_heart", speed=SPEED, lang="en-us")
        assert sr == SR
        a = a.astype(np.float32)
        # trim edge silence
        idx = np.where(np.abs(a) > 0.01)[0]; a = a[idx[0]:idx[-1] + 1]
        gap = GAP_SCENE if j == len(sents) - 1 else GAP_S
        pad = np.zeros(int(gap * SR), np.float32)
        caps.append((s, t, t + len(a) / SR))
        out += [a, pad]; t += (len(a) + len(pad)) / SR
    timing.append(dict(id=sid, title=title, start=s0, end=t, caps=caps))
    print(sid, round(t - s0, 1), flush=True)
lead = 0.8; tail = 1.5
audio = np.concatenate([np.zeros(int(lead * SR), np.float32)] + out + [np.zeros(int(tail * SR), np.float32)])
for sc in timing:
    sc["start"] += lead; sc["end"] += lead
    sc["caps"] = [(s, a + lead, b + lead) for s, a, b in sc["caps"]]
timing[0]["start"] = 0.0; timing[-1]["end"] = len(audio) / SR
sf.write("assets/voice.wav", audio, SR)
json.dump(timing, open("assets/timing.json", "w"), indent=1)
print("TOTAL", len(audio) / SR)
