"""Synthesise a calm, light ambient background track (no external samples)."""
import numpy as np, soundfile as sf
from scipy.signal import fftconvolve
SR = 44100; DUR = 226.0; BPM = 72
rng = np.random.default_rng(7)
beat = 60 / BPM; bar = 4 * beat
def hz(m): return 440.0 * 2 ** ((m - 69) / 12)
# C maj7 | A min7 | F maj7 | G6  (midi)
CH = [[48, 52, 55, 59], [45, 52, 55, 60], [41, 48, 52, 57], [43, 50, 52, 59]]
n = int(DUR * SR); t = np.arange(n) / SR
L = np.zeros(n); R = np.zeros(n)
def add(buf, start, sig):
    i = int(start * SR); e = min(i + len(sig), n)
    if i < n: buf[i:e] += sig[: e - i]
chord_len = 2 * bar
nch = int(DUR / chord_len) + 2
for c in range(nch):
    st = c * chord_len; notes = CH[c % 4]
    ln = chord_len + 2.5; tt = np.arange(int(ln * SR)) / SR
    env = np.minimum(tt / 2.2, 1) * np.clip((ln - tt) / 2.5, 0, 1)
    for k, m in enumerate(notes):
        for dt, pan in ((-0.25, 0.3), (0.25, 0.7)):
            f = hz(m + 12) * 2 ** (dt / 1200 * 8)
            lfo = 1 + 0.15 * np.sin(2 * np.pi * (0.13 + 0.03 * k) * tt + k)
            sig = (np.sin(2 * np.pi * f * tt) + 0.35 * np.sin(2 * np.pi * 2 * f * tt + 1) * lfo + 0.12 * np.sin(2 * np.pi * 3 * f * tt)) * env * 0.045
            add(L, st, sig * (1 - pan)); add(R, st, sig * pan)
    # bass
    tb = np.arange(int(ln * SR)) / SR
    bs = np.sin(2 * np.pi * hz(notes[0] - 12) * tb) * env * 0.10
    add(L, st, bs); add(R, st, bs)
    # arpeggio plucks (eighth notes)
    pattern = [0, 2, 3, 2, 1, 2, 3, 2]
    for j in range(16):
        m = notes[pattern[j % 8]] + 24 + (12 if (j % 8 == 6) else 0)
        ts = st + j * beat / 2
        if ts > DUR: break
        d = int(1.8 * SR); tp = np.arange(d) / SR
        f = hz(m)
        sig = (np.sin(2 * np.pi * f * tp) + 0.4 * np.sin(2 * np.pi * 2 * f * tp) * np.exp(-tp * 6) + 0.15 * np.sin(2 * np.pi * 3 * f * tp) * np.exp(-tp * 10))
        sig *= np.exp(-tp * 2.8) * np.minimum(tp / 0.006, 1) * (0.05 + 0.02 * ((j % 4) == 0))
        pan = 0.25 + 0.5 * ((j * 0.37) % 1)
        add(L, ts, sig * (1 - pan) * 1.4); add(R, ts, sig * pan * 1.4)
# gentle intro and outro
fade = np.minimum(t / 3.0, 1) * np.clip((DUR - t) / 6.0, 0, 1)
L *= fade; R *= fade
# reverb
irn = int(2.8 * SR); ti = np.arange(irn) / SR
ir = rng.standard_normal((2, irn)) * np.exp(-ti * 2.0); ir[:, :int(0.02 * SR)] *= np.linspace(0, 1, int(0.02 * SR))
wl = fftconvolve(L, ir[0] / np.sqrt(irn) * 0.12)[:n]; wr = fftconvolve(R, ir[1] / np.sqrt(irn) * 0.12)[:n]
L = 0.65 * L + 0.55 * wl; R = 0.65 * R + 0.55 * wr
out = np.stack([L, R], 1); out /= np.abs(out).max() * 1.25
sf.write("assets/music.wav", out.astype(np.float32), SR)
print("ok", out.shape)
