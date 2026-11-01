"""Tiny orthographic ray-marcher for the flanged hub (SDF solid modelling)."""
import numpy as np
from PIL import Image

BIG = 1e4
HOLES = [(0, 45), (45, 0), (0, -45), (-45, 0)]
TARGET = np.array([0, 0, 14.0], np.float32)

def cam(az, el):
    a, e = np.radians(az), np.radians(el)
    eye = np.array([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)], np.float32)
    f = -eye
    r = np.cross(f, [0, 0, 1]); r /= np.linalg.norm(r)
    u = np.cross(r, f)
    return eye, f, r.astype(np.float32), u.astype(np.float32)

def project(pts, az, el, S, cx, cy):
    """world points (N,3) -> pixel coords (N,2) (1x viewport pixels)."""
    _, _, r, u = cam(az, el)
    q = np.asarray(pts, np.float32) - TARGET
    return np.stack([cx + S * q @ r, cy - S * q @ u], 1)

def _cyl(x, y, z, r, z0, z1, cx=0.0, cy=0.0):
    dx = np.hypot(x - cx, y - cy) - r
    dz = np.abs(z - (z0 + z1) / 2) - (z1 - z0) / 2
    return np.minimum(np.maximum(dx, dz), 0) + np.hypot(np.maximum(dx, 0), np.maximum(dz, 0))

def _box(x, y, z, x0, x1, y0, y1, z0, z1):
    dx = np.abs(x - (x0 + x1) / 2) - (x1 - x0) / 2
    dy = np.abs(y - (y0 + y1) / 2) - (y1 - y0) / 2
    dz = np.abs(z - (z0 + z1) / 2) - (z1 - z0) / 2
    out = np.hypot(np.hypot(np.maximum(dx, 0), np.maximum(dy, 0)), np.maximum(dz, 0))
    return out + np.minimum(np.maximum(np.maximum(dx, dy), dz), 0)

def sdf(p, M):
    tf, th, dc, dh, nh = M
    x, y, z = p[:, 0], p[:, 1], p[:, 2]
    d = np.full(len(p), BIG, np.float32)
    if tf > 0.01: d = np.minimum(d, _cyl(x, y, z, 60, 0, tf))
    if th > 0.01: d = np.minimum(d, _cyl(x, y, z, 30, 0, th))
    if dc > 0.01:
        z0 = 40 - dc
        cut = np.minimum(_cyl(x, y, z, 15, z0, 45), _box(x, y, z, -4, 4, 0, 18.3, z0, 45))
        d = np.maximum(d, -cut)
    if dh > 0.01:
        for i in range(nh):
            hx, hy = HOLES[i]
            d = np.maximum(d, -_cyl(x, y, z, 6, 16 - dh, 20, hx, hy))
    return d

def render(M, az=35, el=30, S=4.4, W=840, H=520, ss=1.5):
    """returns RGBA uint8 (H,W,4) of the part only (transparent background)."""
    w, h = int(W * ss), int(H * ss)
    s = S * ss
    eye, f, r, u = cam(az, el)
    xs = (np.arange(w) - w / 2 + 0.5) / s
    ys = -(np.arange(h) - h / 2 + 0.5) / s
    X, Y = np.meshgrid(xs, ys)
    X = X.ravel().astype(np.float32); Y = Y.ravel().astype(np.float32)
    # keep the part fixed on the viewport centre-ish (shift down slightly)
    dperp = np.hypot(X, Y)
    R0 = 72.0
    hit_sph = dperp < R0
    idx = np.where(hit_sph)[0]
    O = TARGET[None] + X[idx, None] * r[None] + Y[idx, None] * u[None] + eye[None] * 200
    t = 200 - np.sqrt(R0**2 - dperp[idx] ** 2)
    tmax = 200 + np.sqrt(R0**2 - dperp[idx] ** 2)
    alive = np.ones(len(idx), bool); hit = np.zeros(len(idx), bool)
    act = np.arange(len(idx))
    for _ in range(110):
        if len(act) == 0: break
        p = O[act] + f[None] * t[act, None]
        d = sdf(p.astype(np.float32), M)
        h_ = d < 0.03
        hit[act[h_]] = True
        t[act] += np.maximum(d, 0.02)
        keep = (~h_) & (t[act] < tmax[act])
        act = act[keep]
    hp = O[hit] + f[None] * t[hit, None]
    eps = 0.08
    def g(dx, dy, dz): return sdf((hp + np.array([dx, dy, dz], np.float32)).astype(np.float32), M)
    n = np.stack([g(eps, 0, 0) - g(-eps, 0, 0), g(0, eps, 0) - g(0, -eps, 0), g(0, 0, eps) - g(0, 0, -eps)], 1)
    n /= (np.linalg.norm(n, axis=1, keepdims=True) + 1e-9)
    L1 = -0.55 * r + 0.65 * u + 0.55 * eye; L1 /= np.linalg.norm(L1)
    L2 = 0.7 * r + 0.2 * u + 0.4 * eye; L2 /= np.linalg.norm(L2)
    diff = np.clip(n @ L1, 0, 1); fill = np.clip(n @ L2, 0, 1)
    refl = 2 * (n @ L1)[:, None] * n - L1[None]
    spec = np.clip(refl @ eye, 0, 1) ** 28
    rim = (1 - np.clip(n @ eye, 0, 1)) ** 2
    base = np.array([0.66, 0.72, 0.82], np.float32)
    col = base[None] * (0.36 + 0.62 * diff[:, None] + 0.14 * fill[:, None]) + 0.22 * spec[:, None]
    col = col - 0.10 * rim[:, None]
    # outline pass from depth & normal discontinuities
    D = np.full(w * h, np.nan, np.float32); N = np.zeros((w * h, 3), np.float32)
    full_idx = idx[hit]
    D[full_idx] = t[hit]; N[full_idx] = n
    D = D.reshape(h, w); N = N.reshape(h, w, 3)
    img = np.zeros((h, w, 4), np.float32)
    flat = np.zeros((w * h, 3), np.float32); flat[full_idx] = col
    img[..., :3] = flat.reshape(h, w, 3)
    A = np.zeros(w * h, np.float32); A[full_idx] = 1; A = A.reshape(h, w); img[..., 3] = A
    edge = np.zeros((h, w), bool)
    for ax in (0, 1):
        a = np.roll(D, 1, ax); b = np.roll(N, 1, ax)
        dd = np.abs(D - a)
        edge |= (np.nan_to_num(dd, nan=0) > 1.2)
        edge |= ((N * b).sum(-1) < 0.7) & (A > 0) & (np.roll(A, 1, ax) > 0)
    edge &= A > 0
    ec = np.array([0.10, 0.14, 0.22], np.float32)
    img[edge, :3] = 0.25 * img[edge, :3] + 0.75 * ec
    out = (np.clip(img, 0, 1) * 255).astype(np.uint8)
    im = Image.fromarray(out, "RGBA").resize((W, H), Image.LANCZOS)
    return im

if __name__ == "__main__":
    import time
    t0 = time.time()
    im = render((16, 40, 40, 17, 4)); print(time.time() - t0)
    bg = Image.new("RGBA", im.size, (235, 240, 246, 255)); bg.alpha_composite(im); bg.save("/tmp/test3d.png")
