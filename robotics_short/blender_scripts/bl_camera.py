"""Cinematic camera: per-frame keyed orbit/push shots with a look-at target."""
import math
import bpy
from mathutils import Vector
from bl_common import *
import motion
from config import *    # noqa

P0 = tuple(motion.P0)

# f0, f1, target0, target1, az0, az1, el0, el1, dist0, dist1, lens, fstop0, fstop1, shift_y, ease
SHOTS = [
    (0,    210,  P0, (0.50, 0.0, 0.80), -30, -22, 14, 18, 0.55, 3.2, 50, 4.0, 8.0, -0.04, "io"),
    (210,  600,  P0, P0,                -40, -24, 20, 20, 1.45, 1.40, 50, 8.0, 8.0, -0.20, "lin"),
    (600, 1140,  P0, P0,                -44, -70, 24, 30, 1.50, 1.50, 50, 8.0, 8.0, -0.20, "lin"),
    (1140, 1560, P0, (0.90, 0.0, 0.46), -62, -80, 33, 38, 1.40, 1.20, 50, 8.0, 8.0, -0.20, "lin"),
    (1560, 1620, (0.90, 0.0, 0.42), (0.90, 0.0, 0.40), -30, -46, 26, 30, 1.20, 1.00, 50, 5.6, 5.6, -0.04, "io"),
    (1620, 1680, (0.92, 0.45, 0.38), (0.92, 0.65, 0.38), -34, -6, 20, 22, 0.95, 0.85, 50, 5.6, 5.6, -0.04, "io"),
    (1680, 1740, (0.90, -0.50, 0.48), (0.90, -0.45, 0.50), 55, 75, 28, 30, 1.80, 1.65, 50, 5.0, 5.0, -0.04, "io"),
    (1740, 1800, P0, P0,                -42, -30, 10, 14, 0.90, 0.55, 50, 4.0, 4.0, -0.04, "io"),
]


def _e(kind, u):
    return motion.ease(u) if kind == "io" else u


def build():
    cam_d = bpy.data.cameras.new("Cam")
    cam_d.sensor_fit = "HORIZONTAL"
    cam_d.sensor_width = 36
    cam_d.clip_start, cam_d.clip_end = 0.02, 60
    cam = bpy.data.objects.new("Camera", cam_d)
    link(cam)
    tgt = empty("cam_target", loc=P0, size=0.05)
    c = cam.constraints.new("TRACK_TO")
    c.target, c.track_axis, c.up_axis = tgt, "TRACK_NEGATIVE_Z", "UP_Y"
    cam_d.dof.use_dof = True
    cam_d.dof.focus_object = tgt
    bpy.context.scene.camera = cam
    return cam, tgt


def animate(cam, tgt):
    for (f0, f1, t0, t1, a0, a1, e0, e1, d0, d1, lens, fs0, fs1, sy, ek) in SHOTS:
        for f in range(f0, f1):
            u = _e(ek, (f - f0) / float(f1 - f0 - 1))
            T = Vector([motion.lerp(a, b, u) for a, b in zip(t0, t1)])
            az = math.radians(motion.lerp(a0, a1, u))
            el = math.radians(motion.lerp(e0, e1, u))
            d = motion.lerp(d0, d1, u)
            pos = T + d * Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
            key_vec(cam, "location", f, pos)
            key_vec(tgt, "location", f, T)
            key(cam, "data.lens", f, lens)
            key(cam, "data.shift_y", f, sy)
            key(cam, "data.dof.aperture_fstop", f, motion.lerp(fs0, fs1, u))
