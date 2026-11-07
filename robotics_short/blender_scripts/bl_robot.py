"""Six-axis robot: modelling, joint rig (empties) and tooling.

The joint empties replicate kinematics.CHAIN exactly, so `tool_frame` in
Blender == fk(q)."""
import math
import bpy
from bl_common import *
from bl_materials import *
import kinematics as K
import motion
from config import *     # noqa

AXIS_IDX = {"x": 0, "y": 1, "z": 2}


def build():
    R = {}
    pa, po, st, pl = paint(), polymer(), steel(), part_metal()
    base = empty("robot_base")
    R["base"] = base
    # ---- static pedestal
    mesh_obj("base_plate", bm_cyl(0.36, 0.05, center=(0, 0, 0.025)), po, base, link_id=0)
    mesh_obj("base_ring", bm_cyl(0.30, 0.02, center=(0, 0, 0.06)), st, base, link_id=0)
    mesh_obj("pedestal", bm_cyl(0.24, 0.31, center=(0, 0, 0.225), r2=0.22), pa, base, link_id=0)
    mesh_obj("pedestal_collar", bm_cyl(0.235, 0.03, center=(0, 0, 0.385)), po, base, link_id=0)

    joints = []
    parent = base
    for i, ((off, ax), name) in enumerate(zip(K.CHAIN, ["J1", "J2", "J3", "J4", "J5", "J6"])):
        j = empty(name, parent, loc=off, size=0.08)
        joints.append(j)
        parent = j
    R["joints"] = joints
    j1, j2, j3, j4, j5, j6 = joints

    # ---- link 1 : turret + shoulder motor
    mesh_obj("L1_turret_ring", bm_cyl(0.22, 0.05, center=(0, 0, 0.03)), po, j1, link_id=1)
    mesh_obj("L1_turret", bm_box((0.38, 0.30, 0.34), (0.10, 0, 0.21), 0.03, taper=0.92), pa, j1, link_id=1)
    mesh_obj("L1_motor", bm_cyl(0.145, 0.40, "y", (0.18, 0, 0.22)), po, j1, link_id=1)
    for s in (-1, 1):
        mesh_obj(f"L1_cap{s}", bm_cyl(0.095, 0.02, "y", (0.18, s * 0.205, 0.22)), st, j1, link_id=1)
    # ---- link 2 : upper arm
    mesh_obj("L2_hub", bm_cyl(0.125, 0.24, "y", (0, 0, 0)), pa, j2, link_id=2)
    mesh_obj("L2_arm", bm_box((0.19, 0.20, 0.66), (0, 0, 0.42), 0.035, taper=0.82), pa, j2, link_id=2)
    mesh_obj("L2_elbow_hub", bm_cyl(0.125, 0.26, "y", (0, 0, 0.78)), pa, j2, link_id=2)
    mesh_obj("L2_stripe", bm_box((0.195, 0.205, 0.035), (0, 0, 0.30), 0.004), paint_orange(), j2, link_id=2)
    # ---- link 3 : forearm
    mesh_obj("L3_elbow_cap", bm_cyl(0.10, 0.30, "y", (0, 0, 0)), po, j3, link_id=3)
    mesh_obj("L3_forearm", bm_box((0.52, 0.17, 0.17), (0.31, 0, 0), 0.03), pa, j3, link_id=3)
    mesh_obj("L3_wrist_ring", bm_cyl(0.105, 0.17, "x", (0.61, 0, 0)), po, j3, link_id=3)
    # ---- link 4 : wrist housing (rolls with J4)
    mesh_obj("L4_barrel", bm_cyl(0.085, 0.13, "x", (0.065, 0, 0)), pa, j4, link_id=4)
    mesh_obj("L4_pitch_housing", bm_cyl(0.08, 0.20, "y", (0.14, 0, 0)), po, j4, link_id=4)
    for s in (-1, 1):
        mesh_obj(f"L4_cap{s}", bm_cyl(0.055, 0.02, "y", (0.14, s * 0.105, 0)), st, j4, link_id=4)
    # ---- link 5 : wrist body (pitches with J5)
    mesh_obj("L5_body", bm_box((0.10, 0.11, 0.11), (0.055, 0, 0), 0.02), pa, j5, link_id=5)
    mesh_obj("L5_neck", bm_cyl(0.058, 0.04, "x", (0.10, 0, 0)), po, j5, link_id=5)
    # ---- link 6 : flange
    mesh_obj("L6_flange", bm_cyl(0.062, 0.03, "x", (0.015, 0, 0)), st, j6, link_id=6)
    mesh_obj("L6_flange_plate", bm_cyl(0.075, 0.012, "x", (0.036, 0, 0)), st, j6, link_id=6)

    # ---- tool frame (origin = TCP, tool -Z = approach)
    tool = empty("tool_frame", j6, loc=K.TCP_OFFSET, rot=(0, -math.pi / 2, 0), kind="ARROWS", size=0.08)
    R["tool"] = tool

    # gripper (points along tool -Z; flange sits at tool z=+0.26)
    g = []
    g.append(mesh_obj("G_adapter", bm_cyl(0.062, 0.03, center=(0, 0, 0.238)), po, tool, link_id=7))
    g.append(mesh_obj("G_body", bm_box((0.13, 0.12, 0.075), (0, 0, 0.193), 0.012), pa, tool, link_id=7))
    g.append(mesh_obj("G_rail", bm_box((0.10, 0.27, 0.028), (0, 0, 0.14), 0.008), st, tool, link_id=7))
    fingers = []
    for s in (-1, 1):
        f = mesh_obj(f"G_finger{s}", bm_box((0.05, 0.025, 0.145), (0, 0, 0.0), 0.005), pl if False else st,
                     tool, loc=(0, s * 0.07, 0.065), link_id=7)
        fingers.append((f, s))
        g.append(f)
        g.append(mesh_obj(f"G_pad{s}", bm_box((0.05, 0.006, 0.05), (0, -s * 0.0155, -0.045), 0.002),
                          po, f, link_id=7))
    R["gripper_objs"], R["fingers"] = g, fingers

    # welding torch
    t = []
    t.append(mesh_obj("T_adapter", bm_cyl(0.06, 0.03, center=(0, 0, 0.238)), po, tool, link_id=7))
    t.append(mesh_obj("T_body", bm_cyl(0.034, 0.17, center=(0, 0, 0.14)), po, tool, link_id=7))
    t.append(mesh_obj("T_collar", bm_cyl(0.040, 0.03, center=(0, 0, 0.19)), st, tool, link_id=7))
    t.append(mesh_obj("T_nozzle", bm_cyl(0.034, 0.075, center=(0, 0, 0.0375), r2=0.016), st, tool, link_id=7))
    R["torch_objs"] = t
    R["torch_glow"] = mesh_obj("T_glow", bm_sphere(0.016, (0, 0, 0.0), 16),
                               emissive("weld", (1.0, 0.75, 0.4), 40.0), tool, link_id=7)
    R["torch_glow"].visible_shadow = False

    # parts ---------------------------------------------------------------
    bracket = mesh_obj("part_bracket", bm_box((0.26, 0.12, 0.07), (0, 0, 0), 0.010), pl, tool, link_id=8)
    for sx in (-0.08, 0.08):
        mesh_obj(f"part_hole{sx}", bm_cyl(0.022, 0.004, center=(sx, 0, 0.0355)), po, bracket, link_id=8)
    mesh_obj("part_stripe", bm_box((0.04, 0.121, 0.0715), (0, 0, 0), 0.002), paint_orange(), bracket, link_id=8)
    R["bracket"] = bracket
    cube = mesh_obj("part_cube", bm_box((0.10, 0.10, 0.10), (0, 0, 0), 0.008), cube_part(), None, link_id=8)
    mesh_obj("cube_mark", bm_box((0.04, 0.012, 0.102), (0.0, 0.0, 0.0), 0.002), paint_orange(), cube, link_id=8)
    R["cube"] = cube
    return R


def animate(R, total=TOTAL_FRAMES):
    q = motion.trajectory()
    for f in range(total):
        for i, ((_, ax), j) in enumerate(zip(K.CHAIN, R["joints"])):
            key(j, "rotation_euler", f, float(q[f][i]), AXIS_IDX[ax], "LINEAR")
    # tool visibility
    for o in R["gripper_objs"]:
        if o.parent is R["tool"]:
            set_visible_from_fn(o, lambda f: motion.tool_kind(f) == "gripper", total)
    for o in R["torch_objs"] + [R["torch_glow"]]:
        set_visible_from_fn(o, lambda f: motion.tool_kind(f) == "torch", total)
    set_visible_from_fn(R["bracket"], motion.bracket_visible, total)
    set_visible_from_fn(R["cube"], motion.cube_visible, total)
    # hide_render does not propagate to children -> key every child explicitly
    for ob in list(bpy.data.objects):
        if ob.parent is R["bracket"]:
            set_visible_from_fn(ob, motion.bracket_visible, total)
        elif ob.parent is R["cube"]:
            set_visible_from_fn(ob, motion.cube_visible, total)
        elif ob.parent in [f for f, _ in R["fingers"]]:
            set_visible_from_fn(ob, lambda f: motion.tool_kind(f) == "gripper", total)
    # fingers (local Y of tool): inner face at +-opening, finger half-thickness 0.0125
    for fobj, s in R["fingers"]:
        for f in range(total):
            key(fobj, "location", f, s * (motion.gripper_opening(f) + 0.0185), 1, "LINEAR")
    # carried cube : keyed in world space (parent-less)
    for f in range(motion.S5C - 1, motion.S5D + 1):
        p, yaw = motion.cube_pose(f)
        key_vec(R["cube"], "location", f, p)
        key(R["cube"], "rotation_euler", f, math.radians(yaw), 2, "LINEAR")
    # torch glow: gentle, non-strobing flicker only while welding
    for f in range(motion.S5B, motion.S5C):
        s = 1.0 + 0.12 * math.sin(f * 0.9) + 0.07 * math.sin(f * 2.3)
        key_vec(R["torch_glow"], "scale", f, (s, s, s))
