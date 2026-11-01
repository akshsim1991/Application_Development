import os, sys, math, time
from multiprocessing import Pool
import frames as F
N = None
def keys_job(_):
    pass
def render_key(k):
    M, az, el = k
    import hashlib, r3d
    key = f"{M}_{az}_{el}"; fn = os.path.join(F.CACHE, hashlib.md5(key.encode()).hexdigest() + ".png")
    if not os.path.exists(fn): r3d.render(M, az, el).save(fn)
    return 1
_r3 = None
def frame_job(k):
    global _r3
    if _r3 is None: _r3 = F.R3()
    t = k / F.FPS
    F.compose(t, _r3).save(f"/tmp/frames/{k:05d}.png", compress_level=1)
    return k
if __name__ == "__main__":
    os.makedirs("/tmp/frames", exist_ok=True)
    total = F.TIMING[-1]["end"]; N = int(total * F.FPS)
    print("frames", N, flush=True)
    dry = F.R3(dry=True)
    for k in range(N): F.compose(k / F.FPS, dry, dry=True)
    keys = sorted(dry.keys, key=str); print("3D renders", len(keys), flush=True)
    t0 = time.time()
    with Pool(4) as p:
        for n, _ in enumerate(p.imap_unordered(render_key, keys, chunksize=2)):
            if n % 50 == 0: print("3d", n, round(time.time() - t0), flush=True)
        print("compose", flush=True)
        for n, _ in enumerate(p.imap_unordered(frame_job, range(N), chunksize=8)):
            if n % 300 == 0: print("frame", n, round(time.time() - t0), flush=True)
    print("DONE", flush=True)
