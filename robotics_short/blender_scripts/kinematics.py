"""Six-axis manipulator kinematics (numpy only).

The SAME chain description is used to build the Blender joint hierarchy, so the
animated tool frame in Blender must equal fk() here (verified in qa.py).

Zero pose: upper arm vertical (+Z), forearm horizontal (+X), flange axis +X.
Joint rotations are right-handed about the joint axis in the joint frame.
Tool frame: origin = TCP, tool -Z = approach direction (flange +X).  At
J = (0,0,0,0,90deg,0) with the arm in the XZ plane the tool frame equals the
world frame (identity), so "tool local axes == world axes" at R = I.
"""
import math
import numpy as np

# (offset in parent frame, joint axis)
CHAIN = [
    ((0.00, 0.0, 0.40), "z"),   # J1 base swivel
    ((0.18, 0.0, 0.22), "y"),   # J2 shoulder
    ((0.00, 0.0, 0.78), "y"),   # J3 elbow
    ((0.70, 0.0, 0.00), "x"),   # J4 forearm roll
    ((0.14, 0.0, 0.00), "y"),   # J5 wrist pitch
    ((0.12, 0.0, 0.00), "x"),   # J6 flange roll
]
TCP_OFFSET = (0.26, 0.0, 0.0)        # along flange +X
LIMITS = [(-2.9, 2.9), (-1.2, 1.9), (-1.3, 2.2), (-3.1, 3.1),
          (-2.0, 2.0), (-7.0, 7.0)]  # radians (J6 allows >360 deg of roll)
HOME_Q = np.array([0.0, 0.55, 0.85, 0.0, 1.0, 0.0])


def rot(axis, a):
    c, s = math.cos(a), math.sin(a)
    if axis == "x":
        return np.array([[1, 0, 0], [0, c, -s], [0, s, c]], float)
    if axis == "y":
        return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]], float)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]], float)


MOUNT = rot("y", -math.pi / 2)   # tool = flange * MOUNT


def T(R=None, p=None):
    M = np.eye(4)
    if R is not None:
        M[:3, :3] = R
    if p is not None:
        M[:3, 3] = p
    return M


def fk(q):
    """Return (list of 4x4 joint frames, tool 4x4)."""
    M = np.eye(4)
    frames = []
    for (off, ax), a in zip(CHAIN, q):
        M = M @ T(None, off) @ T(rot(ax, a))
        frames.append(M.copy())
    tool = M @ T(None, TCP_OFFSET) @ T(MOUNT)
    return frames, tool


def rot_err(Rt, Rc):
    """Rotation-vector of Rt * Rc^T (world frame)."""
    E = Rt @ Rc.T
    c = (np.trace(E) - 1) / 2
    ang = math.acos(max(-1, min(1, c)))
    v = np.array([E[2, 1] - E[1, 2], E[0, 2] - E[2, 0], E[1, 0] - E[0, 1]])
    if ang < 1e-9:
        return 0.5 * v
    if abs(math.pi - ang) < 1e-4:
        return v / 2 * (ang / max(math.sin(ang), 1e-4))
    return v * (ang / (2 * math.sin(ang)))


def ik(p, R, q0, iters=60, lam=0.01, bias=0.0):
    q = np.array(q0, float)
    lo = np.array([l for l, _ in LIMITS]); hi = np.array([h for _, h in LIMITS])
    for _ in range(iters):
        _, Tt = fk(q)
        e = np.concatenate([p - Tt[:3, 3], rot_err(R, Tt[:3, :3])])
        if np.linalg.norm(e[:3]) < 2e-6 and np.linalg.norm(e[3:]) < 2e-6:
            break
        J = np.zeros((6, 6))
        h = 1e-6
        for i in range(6):
            dq = q.copy(); dq[i] += h
            _, T2 = fk(dq)
            J[:3, i] = (T2[:3, 3] - Tt[:3, 3]) / h
            J[3:, i] = rot_err(T2[:3, :3], Tt[:3, :3]) / h
        A = J.T @ J + lam * lam * np.eye(6)
        # weak posture bias keeps the solution on the elbow-up branch
        g = J.T @ e + bias * (HOME_Q - q) * np.array([0, 1, 1, 0, 1, 0])
        q = np.clip(q + np.linalg.solve(A, g), lo, hi)
    _, Tt = fk(q)
    err = (np.linalg.norm(p - Tt[:3, 3]), np.linalg.norm(rot_err(R, Tt[:3, :3])))
    return q, err
