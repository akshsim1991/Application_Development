"""Coordinate-system animation: fixed world axes at the TCP, rotating tool axes,
right-hand-rule rotation arcs and the highlighted transformed x-axis."""
import math
import os
import bpy
from mathutils import Vector
from bl_common import *
from bl_materials import emissive
import motion
from config import *     # noqa

AX = {"x": Vector((1, 0, 0)), "y": Vector((0, 1, 0)), "z": Vector((0, 0, 1))}
COL = {"x": C_X, "y": C_Y, "z": C_Z}
UV = {"x": ("y", "z"), "y": ("z", "x"), "z": ("x", "y")}      # right-handed cyclic
IDX = {"x": 0, "y": 1, "z": 2}
_font = None


def font():
    global _font
    if _font is None:
        fp = path("assets", "fonts", "Inter-Bold.otf")
        _font = bpy.data.fonts.load(fp) if os.path.exists(fp) else None
    return _font


def arrow_mesh(axis, L, r, tip=0.14):
    import bmesh
    bm = bmesh.new()
    sh = L * (1 - tip)
    bmesh.ops.create_cone(bm, cap_ends=True, segments=20, radius1=r, radius2=r, depth=sh)
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector((0, 0, sh / 2)))
    n0 = len(bm.verts)
    bmesh.ops.create_cone(bm, cap_ends=True, segments=20, radius1=r * 2.8, radius2=0.0, depth=L * tip)
    new = bm.verts[n0:]
    bmesh.ops.translate(bm, verts=new, vec=Vector((0, 0, sh + L * tip / 2)))
    from bl_common import _axis_rot
    bmesh.ops.transform(bm, matrix=_axis_rot(axis), verts=bm.verts)
    return bm


def label(text, parent, loc, size, mat, cam):
    cu = bpy.data.curves.new("lbl_" + text, "FONT")
    cu.body, cu.size, cu.align_x, cu.align_y = text, size, "CENTER", "CENTER"
    cu.extrude = 0.002
    if font():
        cu.font = font()
    ob = bpy.data.objects.new("lbl_" + text, cu)
    ob.location = loc
    cu.materials.append(mat)
    link(ob, parent)
    c = ob.constraints.new("TRACK_TO")
    c.target, c.track_axis, c.up_axis = cam, "TRACK_Z", "UP_Y"
    ob.visible_shadow = False
    return ob


def make_arc(name, ax, r, span_deg, color, parent, strength=1.6, thick=0.0075):
    u, v = AX[UV[ax][0]], AX[UV[ax][1]]
    N = 96
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth, cu.bevel_resolution, cu.use_fill_caps = thick, 3, True
    sp = cu.splines.new("POLY")
    sp.points.add(N - 1)
    for i, p in enumerate(sp.points):
        a = math.radians(span_deg) * i / (N - 1)
        co = r * (math.cos(a) * u + math.sin(a) * v)
        p.co = (co.x, co.y, co.z, 1.0)
    cu.bevel_factor_start, cu.bevel_factor_end = 0.0, 0.0
    mat = emissive("arc_" + name, color, strength)
    cu.materials.append(mat)
    arc = bpy.data.objects.new(name, cu)
    link(arc, parent)
    arc.visible_shadow = False
    pivot = empty(name + "_pivot", parent)
    cone = mesh_obj(name + "_head", _cone(thick * 3.6, thick * 8.0), mat, pivot, loc=tuple(r * u))
    cone.rotation_mode = "QUATERNION"
    cone.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(v)
    cone.visible_shadow = False
    return dict(arc=arc, pivot=pivot, cone=cone, span=span_deg, ax=ax)


def _cone(r, h):
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=r, radius2=0.0, depth=h)
    return bm


def animate_arc(A, angle_fn, windows, total):
    """angle_fn(f)-> degrees (positive = right-hand rotation about the axis)."""
    ax = A["ax"]
    on = [False] * total
    for a, b in windows:
        for f in range(a, min(b, total)):
            on[f] = True
    for f in range(total):
        if not on[f] and not (f > 0 and on[f - 1]) and not (f + 1 < total and on[f + 1]):
            continue
        ang = angle_fn(f) if on[f] else 0.0
        ang = max(0.0, ang)
        key(A["arc"], "data.bevel_factor_end", f, min(1.0, ang / A["span"]))
        key(A["pivot"], "rotation_euler", f, math.radians(ang), IDX[ax])
        k = min(1.0, ang / 6.0)
        key_vec(A["cone"], "scale", f, (k, k, k))
    for o in (A["arc"], A["cone"]):
        show_window(o, windows, total)


def build(cam, R, total=TOTAL_FRAMES):
    G = {}
    fixed = empty("fixed_frame", loc=tuple(motion.P0), kind="ARROWS", size=0.1)
    for f in range(S2, S5):
        key_vec(fixed, "location", f, motion.position(f))
    G["fixed"] = fixed
    tool = R["tool"]

    objs_f, objs_l = [], []
    for a in "xyz":
        m = emissive("axisF_" + a, COL[a], 1.0)
        objs_f.append(mesh_obj(f"axisF_{a}", arrow_mesh(a, 0.48, 0.0048), m, fixed, smooth=0))
        objs_f.append(label(a.upper(), fixed, tuple(AX[a] * 0.56), 0.075, emissive("lblF_" + a, COL[a], 1.5), cam))
        m2 = emissive("axisL_" + a, COL[a], 1.6)
        objs_l.append(mesh_obj(f"axisL_{a}", arrow_mesh(a, 0.34, 0.0105), m2, tool, smooth=0))
    # tool frame axes are expressed in tool-frame coordinates -> identity orientation at R = I
    objs_l.append(mesh_obj("axisL_origin", bm_sphere(0.022), emissive("orig", (1, 1, 1), 1.2), tool, smooth=0))
    lbl = emissive("lblL", (1.0, 1.0, 1.0), 4.0)
    for a in "xyz":
        objs_l.append(label(a + "′", tool, tuple(AX[a] * 0.42), 0.06, emissive("lblL_" + a, COL[a], 1.6), cam))
    for o in objs_f + objs_l:
        o.visible_shadow = False
    for o in objs_f + objs_l:
        show_window(o, [(S2, S5)], total)
    G["fixed_objs"], G["local_objs"] = objs_f, objs_l

    # highlighted transformed x-axis (warm orange) during the 90-degree demonstration
    hl = mesh_obj("xprime_highlight", arrow_mesh("x", 0.44, 0.017), emissive("hl", C_ORANGE, 2.2), tool, smooth=0)
    hl.visible_shadow = False
    show_window(hl, [(motion.S4_T0 - 6, S5)], total)
    # vector markers  v = [1,0,0]^T (fixed)  and  v' = R v  (on x')
    vg = mesh_obj("v_ghost", bm_sphere(0.026), emissive("vg", (1, 1, 1), 1.6), fixed, loc=(0.34, 0, 0), smooth=0)
    vp = mesh_obj("v_prime", bm_sphere(0.03), emissive("vp", C_ORANGE, 2.2), tool, loc=(0.34, 0, 0), smooth=0)
    for o in (vg, vp):
        o.visible_shadow = False
    show_window(vg, [(S3 + 90, S5)], total)
    show_window(vp, [(S3 + 90, S5)], total)
    G["vec"] = (vg, vp)

    # rotation arcs --------------------------------------------------------
    arcs = []
    for ax, st, pk in motion.S2_DEMOS:
        A = make_arc("arc_" + ax, ax.lower(), 0.30, pk + 12, COL[ax.lower()], fixed)
        animate_arc(A, lambda f, st=st, pk=pk: motion.demo_angle(f, st, pk), [(st + 14, st + 125)], total)
        arcs.append(A)
    A3 = make_arc("arc_s3", "z", 0.30, 360, C_Z, fixed, 1.4)
    animate_arc(A3, lambda f: motion.theta_z_deg(f) or 0.0, [(motion.S3_T0 - 10, motion.S3_T1 + 14)], total)
    A4 = make_arc("arc_s4", "z", 0.30, 90, C_ORANGE, fixed, 2.0, 0.01)
    animate_arc(A4, lambda f: motion.theta_z_deg(f) or 0.0, [(motion.S4_T0 - 4, S5)], total)
    G["arcs"] = arcs + [A3, A4]
    return G
