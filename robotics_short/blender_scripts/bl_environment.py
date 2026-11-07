"""Dark industrial studio + work cell (table, fixture, weld joint, pads)."""
import math
import bpy
import bmesh
from mathutils import Vector
from bl_common import *
from bl_materials import *
import motion
from config import *     # noqa


def _inward_cylinder(r, h, segs=64):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=False, segments=segs, radius1=r, radius2=r, depth=h)
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector((0, 0, h / 2)))
    return bm


def build():
    E = {}
    # floor + studio wall
    bm = bmesh.new()
    bmesh.ops.create_circle(bm, cap_ends=True, cap_tris=False, segments=96, radius=7.0)
    E["floor"] = mesh_obj("floor", bm, floor(), smooth=0)
    E["wall"] = mesh_obj("wall", _inward_cylinder(7.0, 5.0), wall(), smooth=0)
    # LED architecture on the wall (low intensity, subtle colour accents)
    for i in range(18):
        a = i * 2 * math.pi / 18 + 0.17
        col = (0.10, 0.35, 1.0) if i % 3 else (1.0, 0.45, 0.08)
        m = led("blue" if i % 3 else "orange", col, 2.2)
        mesh_obj(f"led_strip{i}", bm_box((0.05, 0.12, 2.6), (6.93 * math.cos(a), 6.93 * math.sin(a), 1.7), 0.0),
                 m, rot=(0, 0, a), smooth=0)
    mesh_obj("led_ring_low", _ring(6.9, 0.04, 0.05), led("blue", (0.10, 0.35, 1.0), 1.6), smooth=0)
    # floor safety ring (orange)
    mesh_obj("safety_ring", _ring(1.95, 0.035, 0.004), led("orange", (1.0, 0.45, 0.08), 3.0), smooth=0)

    # work table
    tab = table()
    mesh_obj("table_top", bm_box((0.95, 1.90, 0.05), (0.95, 0, 0.275), 0.01), tab, link_id=20)
    for sx in (0.55, 1.35):
        for sy in (-0.85, 0.85):
            mesh_obj(f"table_leg_{sx}_{sy}", bm_box((0.06, 0.06, 0.25), (sx, sy, 0.125), 0.004), polymer(), link_id=20)
    # assembly fixture (4 locating posts; pocket long axis = world Y)
    st, po = steel(), polymer()
    fx, fy = motion.P0[0], motion.P0[1]
    mesh_obj("fixture_plate", bm_box((0.30, 0.42, 0.02), (fx, fy, 0.31), 0.004), st, link_id=21)
    for sx in (-1, 1):
        for sy in (-1, 1):
            mesh_obj(f"fixture_post{sx}{sy}", bm_box((0.04, 0.04, 0.06), (fx + sx * 0.09, fy + sy * 0.16, 0.35), 0.004),
                     paint_orange(), link_id=21)
    mesh_obj("fixture_inlay", bm_box((0.14, 0.28, 0.002), (fx, fy, 0.3205), 0.0), led("fx", (1.0, 0.45, 0.08), 1.2),
             link_id=21, smooth=0)
    # weld joint (T-joint)
    wx, wy = 0.90, 0.55
    mesh_obj("weld_base", bm_box((0.34, 0.56, 0.02), (wx, wy, 0.31), 0.003), st, link_id=22)
    mesh_obj("weld_web", bm_box((0.02, 0.50, 0.12), (wx, wy, 0.38), 0.003), st, link_id=22)
    bead = mesh_obj("weld_bead", bm_box((0.014, 1.0, 0.014), (0, 0.5, 0), 0.0), weld_bead(), link_id=22, smooth=0)
    bead.location = (0.919, motion.WELD_A[1], 0.328)
    E["bead"] = bead
    # pick / place pads
    mesh_obj("pick_pad", bm_box((0.18, 0.18, 0.006), (motion.PICK[0], motion.PICK[1], 0.303), 0.0),
             led("orange", (1.0, 0.45, 0.08), 1.6), link_id=23, smooth=0)
    mesh_obj("place_pad", bm_box((0.18, 0.18, 0.006), (motion.PLACE[0], motion.PLACE[1], 0.303), 0.0),
             led("blue", (0.10, 0.35, 1.0), 1.6), link_id=23, smooth=0)
    return E


def _ring(r, w, h, segs=96):
    bm = bmesh.new()
    bmesh.ops.create_circle(bm, cap_ends=False, segments=segs, radius=r + w)
    outer = list(bm.verts)
    ob = bmesh.ops.create_circle(bm, cap_ends=False, segments=segs, radius=r - w)
    inner = [v for v in bm.verts if v not in outer]
    bmesh.ops.bridge_loops(bm, edges=bm.edges[:])
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector((0, 0, h)))
    return bm


def animate(E, total=TOTAL_FRAMES):
    """Weld bead grows with the torch."""
    for f in range(motion.S5B, motion.S5C + 1):
        p = motion.position(f)
        length = max(0.0005, min(0.40, p[1] - motion.WELD_A[1])) if motion.tool_kind(f) == "torch" else 0.0005
        key(E["bead"], "scale", f, length if length > 0.001 else 0.0005, 1, "LINEAR")
    for f in range(0, motion.S5B):
        pass
    set_visible_from_fn(E["bead"], lambda f: f >= motion.S5B, total)
    key(E["bead"], "scale", 0, 0.0005, 1, "CONSTANT")
