"""Single source of truth for the animation: tool pose, angles, part states.

Blender (build_scene.py), the HUD/matrix overlay (hud.py) and the QA checks all
call these functions, so the on-screen matrix is always synchronised with the
3D rotation.  Pure Python + numpy.
"""
import math
import os
import numpy as np
from config import *            # noqa
from kinematics import rot, ik, fk, HOME_Q

D2R = math.pi / 180.0
P0 = np.array([0.90, 0.0, 0.55])           # home TCP
SEAT_Z = 0.36                              # TCP height with part seated in fixture
PICK = np.array([0.75, -0.60, 0.360])      # cube centre on pick pad
PLACE = np.array([1.05, -0.45, 0.360])
WELD_A = np.array([0.935, 0.35, 0.342])
WELD_B = np.array([0.935, 0.75, 0.342])


def clamp01(u):
    return max(0.0, min(1.0, u))


def ease(u):                                # smootherstep
    u = clamp01(u)
    return u * u * u * (u * (6 * u - 15) + 10)


def seg(f, f0, f1):
    return ease((f - f0) / float(f1 - f0))


def lerp(a, b, t):
    return a + (b - a) * t


# ---------------------------------------------------------------- angles
def demo_angle(f, start, peak):
    """hold 20 / up 40 / hold 25 / down 40  (125 frames)."""
    r = f - start
    if r < 20 or r >= 125:
        return 0.0
    if r < 60:
        return peak * ease((r - 20) / 40.0)
    if r < 85:
        return peak
    return peak * (1 - ease((r - 85) / 40.0))


S2_DEMOS = [("X", 225, 45.0), ("Y", 350, 45.0), ("Z", 475, 60.0)]
S3_T0, S3_T1 = 760, 1120
S4_T0, S4_T1 = 1230, 1350
S4_DESC0, S4_DESC1 = 1450, 1530


def theta_z_deg(f):
    """Angle used by the matrix overlay (about fixed world Z)."""
    if f < S2:
        u = f / float(S2)
        return 100.0 * math.sin(math.pi * u) ** 2
    if S3 <= f < S4:
        return 360.0 * seg(f, S3_T0, S3_T1)
    if S4 <= f < S5:
        return 90.0 * seg(f, S4_T0, S4_T1)
    return None


def orientation(f):
    """Active tool rotation R(f) (3x3) w.r.t. the fixed world frame."""
    if f < S2:
        return rot("z", theta_z_deg(f) * D2R)
    if f < S3:
        R = np.eye(3)
        for ax, st, pk in S2_DEMOS:
            a = demo_angle(f, st, pk)
            if a:
                R = rot(ax.lower(), a * D2R)
        return R
    if f < S4:
        return rot("z", theta_z_deg(f) * D2R)
    if f < S5:
        return rot("z", theta_z_deg(f) * D2R)
    if f < S5B:                              # assembly: settle 60->90 deg, tilt out
        u = seg(f, S5, S5B - 8)
        return rot("z", lerp(55, 90, u) * D2R) @ rot("y", lerp(14, 0, u) * D2R)
    if f < S5C:                              # welding: work/travel angles change
        u = seg(f, S5B + 6, S5C - 6)
        return rot("y", lerp(38, 14, u) * D2R) @ rot("x", lerp(-14, 12, u) * D2R)
    if f < S5D:                              # pick & place with 90 deg yaw
        u = seg(f, S5C + 18, S5D - 14)
        return rot("z", 90 * u * D2R)
    u = seg(f, S5D, TOTAL_FRAMES - 1)        # closing: settle back to identity
    return rot("z", 40 * (1 - u) * D2R)


def position(f):
    if f < S4:
        return P0.copy()
    if f < S5:
        d = seg(f, S4_DESC0, S4_DESC1)
        return np.array([P0[0], P0[1], lerp(P0[2], SEAT_Z, d)])
    if f < S5B:
        u = seg(f, S5 + 4, S5B - 8)
        return np.array([P0[0], P0[1], lerp(0.60, SEAT_Z, u)])
    if f < S5C:
        u = seg(f, S5B + 6, S5C - 6)
        return WELD_A + (WELD_B - WELD_A) * u
    if f < S5D:
        t = f - S5C
        if t < 18:
            z = lerp(0.50, PICK[2], ease(t / 18.0)); xy = PICK[:2] if t > 0 else PICK[:2]
            # arrive from above pad
            return np.array([PICK[0], PICK[1], z])
        u = seg(f, S5C + 18, S5D - 14)
        xy = PICK[:2] + (PLACE[:2] - PICK[:2]) * u
        lift = 0.20 * math.sin(math.pi * u) ** 0.8
        return np.array([xy[0], xy[1], PICK[2] + lift])
    u = seg(f, S5D, TOTAL_FRAMES - 1)
    return np.array([P0[0], P0[1], lerp(0.60, P0[2], u)])


# ---------------------------------------------------------------- tool state
def tool_kind(f):
    return "torch" if S5B <= f < S5C else "gripper"


def bracket_visible(f):
    """The held 'bracket' part."""
    if f < S2:
        return True
    if S2 <= f < S4:
        return False
    if S4 <= f < S5C:
        return not (S5B <= f < S5C)
    if f < S5D:
        return False
    return True


def cube_visible(f):
    return S5C <= f < S5D


def gripper_opening(f):
    """Half gap between finger inner faces (m)."""
    if bracket_visible(f):
        return 0.06
    if S5C <= f < S5D:
        t = f - S5C
        if t < 8:
            return lerp(0.085, 0.05, ease(t / 8.0))
        if f > S5D - 10:
            return lerp(0.05, 0.085, ease((f - (S5D - 10)) / 8.0))
        return 0.05
    return 0.07


def cube_pose(f):
    """World position of the transported cube (centre) and yaw (deg)."""
    if f < S5C + 18:
        return PICK.copy(), 0.0
    if f < S5D - 14:
        p = position(f)
        return p, 90 * seg(f, S5C + 18, S5D - 14)
    return PLACE.copy(), 90.0


# ---------------------------------------------------------------- trajectory
_cache = {}


SEED_FRAMES = [0, S2, S3, S4, S5, S5B, S5C, S5D]


def _seed(f):
    p = position(f)
    q = HOME_Q.copy()
    q[0] = math.atan2(p[1], p[0])
    return q


def trajectory():
    """Joint angles (N,6) for every frame via IK (cached on disk)."""
    if "q" in _cache:
        return _cache["q"]
    cache = path("scenes", "trajectory.npz")
    src_mtime = max(os.path.getmtime(__file__),
                    os.path.getmtime(os.path.join(os.path.dirname(__file__), "kinematics.py")))
    if os.path.exists(cache) and os.path.getmtime(cache) > src_mtime:
        d = np.load(cache)
        _cache["q"], _cache["err"] = d["q"], float(d["err"])
        return _cache["q"]
    out, worst, q = [], 0.0, HOME_Q.copy()
    for f in range(TOTAL_FRAMES):
        if f in SEED_FRAMES:
            q = _seed(f)
            for _ in range(8):
                q, _e = ik(position(f), orientation(f), q, iters=200)
        q, err = ik(position(f), orientation(f), q)
        worst = max(worst, err[0], err[1])
        out.append(q.copy())
    _cache["q"], _cache["err"] = np.array(out), worst
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    np.savez(cache, q=_cache["q"], err=worst)
    return _cache["q"]


def ik_worst_error():
    trajectory()
    return _cache["err"]
