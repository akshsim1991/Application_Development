"""Numerical validation of every rotation claim made in the video (no bpy)."""
import json
import math
import sys
import numpy as np
from config import *            # noqa
import motion
from kinematics import rot, fk


def Rz_formula(t):
    c, s = math.cos(t), math.sin(t)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def run():
    res, ok = [], True

    def check(name, cond, detail=""):
        nonlocal ok
        ok &= bool(cond)
        res.append(dict(check=name, passed=bool(cond), detail=detail))

    t = np.radians(np.linspace(-360, 360, 97))
    check("Rz(theta) matches printed matrix for 97 angles",
          all(np.allclose(rot("z", a), Rz_formula(a)) for a in t))
    for ax in "xyz":
        R = rot(ax, 0.7)
        check(f"R{ax} orthonormal, det=+1 (right-handed)",
              np.allclose(R @ R.T, np.eye(3)) and np.isclose(np.linalg.det(R), 1))
    # right-hand rule: +90 deg about X: Y->Z ; about Y: Z->X ; about Z: X->Y
    e = np.eye(3)
    check("Right-hand rule Rx(90): Y->Z", np.allclose(rot("x", math.pi / 2) @ e[1], e[2]))
    check("Right-hand rule Ry(90): Z->X", np.allclose(rot("y", math.pi / 2) @ e[2], e[0]))
    check("Right-hand rule Rz(90): X->Y", np.allclose(rot("z", math.pi / 2) @ e[0], e[1]))
    R90 = np.round(rot("z", math.pi / 2), 12) + 0.0
    check("Rz(90) == [[0,-1,0],[1,0,0],[0,0,1]]",
          np.array_equal(R90, np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1]], float)), R90.tolist())
    v = np.round(rot("z", math.pi / 2) @ np.array([1.0, 0, 0]), 12) + 0.0
    check("Rz(90) x [1,0,0]^T == [0,1,0]^T", np.array_equal(v, [0, 1, 0]), v.tolist())
    # counter-clockwise seen from +Z: rotation angle of (1,0)->(cos,sin) is positive
    check("CCW viewed from +Z: atan2(y,x) of rotated X equals +theta",
          np.isclose(math.atan2(*(rot("z", 0.3) @ e[0])[[1, 0]]), 0.3))

    # trajectory-level checks
    q = motion.trajectory()
    worst = motion.ik_worst_error()
    check("IK reproduces commanded tool pose on all 1800 frames (<1e-5)", worst < 1e-5,
          f"worst={worst:.2e}")
    # forward kinematics of the joint angles that will be keyed in Blender
    err = 0.0
    for f in range(0, TOTAL_FRAMES, 7):
        _, Tt = fk(q[f])
        err = max(err, np.abs(Tt[:3, :3] - motion.orientation(f)).max(),
                  np.abs(Tt[:3, 3] - motion.position(f)).max())
    check("FK(joints) == commanded pose (every 7th frame)", err < 1e-5, f"max={err:.2e}")
    R = motion.orientation(S4_T1_F)
    check("Scene 4: tool local X at end of rotation == world +Y",
          np.allclose(R[:, 0], [0, 1, 0], atol=1e-9), R[:, 0].round(9).tolist())
    R0 = motion.orientation(S4_T0_F)
    check("Scene 4: tool local X before rotation == world +X",
          np.allclose(R0[:, 0], [1, 0, 0], atol=1e-9))
    check("Scene 4: theta_z(end) == 90 deg exactly",
          abs(motion.theta_z_deg(S4_T1_F) - 90.0) < 1e-9)
    check("Orientation at frame 0 and 1799 is identity (loop)",
          np.allclose(motion.orientation(0), np.eye(3)) and np.allclose(motion.orientation(1799), np.eye(3), atol=1e-6))
    # S2: rotations are individual (single-axis) and reset between demos
    only_single = True
    for f in range(S2, S3):
        Rf = motion.orientation(f)
        singles = [np.allclose(Rf, rot(a, math.acos(max(-1, min(1, (np.trace(Rf) - 1) / 2))) * s), atol=1e-6) or np.allclose(Rf, np.eye(3), atol=1e-9)
                   for a in "xyz" for s in (1, -1)]
        only_single &= any(singles) or np.allclose(Rf, np.eye(3))
    check("Scene 2: each frame is identity or a single-axis rotation", only_single)
    check("Scene 2: tool reset to identity between demos",
          all(np.allclose(motion.orientation(f), np.eye(3)) for f in (350, 475)))
    return ok, res


S4_T0_F = motion.S4_T0
S4_T1_F = motion.S4_T1

if __name__ == "__main__":
    ok, res = run()
    for r in res:
        print(("PASS " if r["passed"] else "FAIL ") + r["check"], r["detail"])
    out = path("documentation", "math_validation.json")
    json.dump(dict(all_passed=ok, checks=res), open(out, "w"), indent=2)
    print("ALL PASSED" if ok else "FAILURES", "->", out)
    sys.exit(0 if ok else 1)
