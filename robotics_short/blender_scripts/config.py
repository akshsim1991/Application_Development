"""Project-wide constants. Pure Python (no bpy) so every stage can import it."""
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def path(*parts):
    return os.path.join(ROOT, *parts)


FPS = 30
TOTAL_FRAMES = 1800          # frames 0..1799  -> exactly 60.000 s
DURATION_S = TOTAL_FRAMES / FPS

PRESETS = {
    "preview": dict(w=540, h=960, engine="AUTO_FAST", samples=4, denoise=True,
                    motion_blur=False, dof=True, bounces=2),
    "final": dict(w=2160, h=3840, engine="CYCLES", samples=160, denoise=True,
                  motion_blur=True, dof=True, bounces=6),
}
DELIVERY = (1080, 1920)

# Scene boundaries (frame index where the scene starts)
S1, S2, S3, S4, S5 = 0, 210, 600, 1140, 1560
S5B, S5C, S5D = 1620, 1680, 1740
# Fade-through-dark transitions: (boundary_frame, half_width_in_frames)
DIPS = [(S2, 6), (S3, 6), (S4, 6), (S5, 6), (S5B, 4), (S5C, 4), (S5D, 4)]

# ---- Locked narration (verbatim; never edit) -------------------------------
NARRATION = [
    (0.0, 7.0,
     "Have you ever wondered how an industrial robot knows exactly which way "
     "to turn its gripper?"),
    (7.0, 20.0,
     "In robotics, orientation describes which way a tool is facing. We can "
     "rotate it about three coordinate axes: X, Y, and Z. Each rotation is "
     "described by an angle."),
    (20.0, 38.0,
     "But how do we represent these rotations mathematically? That’s where "
     "rotation matrices come in! A rotation matrix is a three-by-three matrix "
     "containing sine and cosine terms. It allows us to calculate how "
     "coordinates change when an object rotates."),
    (38.0, 52.0,
     "For example, when a robotic gripper rotates ninety degrees about the "
     "Z-axis, its local X-axis points along the original positive Y-direction. "
     "The rotation matrix describes this transformation precisely."),
    (52.0, 60.0,
     "And that’s how industrial robots can calculate tool orientations for "
     "precise assembly, welding, and pick-and-place operations. Follow for "
     "more robotics concepts!"),
]

# Palette (linear-ish sRGB 0-1)
C_X = (0.95, 0.12, 0.10)
C_Y = (0.18, 0.80, 0.22)
C_Z = (0.12, 0.35, 0.98)
C_ORANGE = (1.0, 0.55, 0.08)
