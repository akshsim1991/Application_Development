"""Small bpy helpers shared by all Blender modules."""
import math
import bpy
import bmesh
from mathutils import Vector


def scene():
    return bpy.context.scene


def link(ob, parent=None):
    scene().collection.objects.link(ob)
    if parent is not None:
        ob.parent = parent
    return ob


def empty(name, parent=None, loc=(0, 0, 0), rot=(0, 0, 0), kind="PLAIN_AXES", size=0.05):
    ob = bpy.data.objects.new(name, None)
    ob.empty_display_type = kind
    ob.empty_display_size = size
    ob.location, ob.rotation_euler = loc, rot
    return link(ob, parent)


# ---------------------------------------------------------------- meshes
def _axis_rot(axis):
    """Rotation matrix sending +Z to the requested axis."""
    from mathutils import Matrix
    return {"z": Matrix.Identity(4),
            "x": Matrix.Rotation(math.pi / 2, 4, "Y"),
            "y": Matrix.Rotation(-math.pi / 2, 4, "X")}[axis]


def bm_box(size, center=(0, 0, 0), bevel=0.008, taper=1.0):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        z = v.co.z + 0.5                       # 0 bottom .. 1 top
        k = 1.0 + (taper - 1.0) * z
        v.co = Vector((v.co.x * size[0] * k, v.co.y * size[1] * k, v.co.z * size[2]))
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel, segments=2, profile=0.6,
                        affect="EDGES")
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector(center))
    return bm


def bm_cyl(r, h, axis="z", center=(0, 0, 0), segs=40, r2=None):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r,
                          radius2=r if r2 is None else r2, depth=h)
    bmesh.ops.transform(bm, matrix=_axis_rot(axis), verts=bm.verts)
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector(center))
    return bm


def bm_sphere(r, center=(0, 0, 0), segs=24):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=segs // 2, radius=r)
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector(center))
    return bm


def mesh_obj(name, bm, mat=None, parent=None, loc=(0, 0, 0), rot=(0, 0, 0), smooth=32, link_id=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.shade_smooth()
    if smooth:
        me.set_sharp_from_angle(angle=math.radians(smooth))
    ob = bpy.data.objects.new(name, me)
    ob.location, ob.rotation_euler = loc, rot
    if mat is not None:
        me.materials.append(mat)
    if link_id is not None:
        ob["link"] = link_id
    return link(ob, parent)


# ---------------------------------------------------------------- animation
def _interp(kind):
    bpy.context.preferences.edit.keyframe_new_interpolation_type = kind


def key(ob, data_path, frame, value, index=-1, interp="LINEAR"):
    _interp(interp)
    owner, rel = ob, data_path
    if data_path.startswith("data."):          # property lives on the object-data ID
        owner, rel = ob.data, data_path[5:]
    target = owner
    parts = rel.split(".")
    for p in parts[:-1]:
        target = getattr(target, p)
    if index >= 0:
        getattr(target, parts[-1])[index] = value
    else:
        setattr(target, parts[-1], value)
    owner.keyframe_insert(data_path=rel, index=index, frame=frame)


def key_vec(ob, data_path, frame, vec, interp="LINEAR"):
    _interp(interp)
    setattr(ob, data_path, tuple(vec))
    ob.keyframe_insert(data_path=data_path, frame=frame)


def show_window(ob, ranges, total):
    """Visible only inside the given [(f0, f1), ...] windows (inclusive f0, exclusive f1)."""
    flags = [False] * total
    for a, b in ranges:
        for f in range(a, min(b, total)):
            flags[f] = True
    last = None
    for f in range(total):
        if flags[f] != last:
            for attr in ("hide_render", "hide_viewport"):
                _interp("CONSTANT")
                setattr(ob, attr, not flags[f])
                ob.keyframe_insert(data_path=attr, frame=f)
            last = flags[f]


def set_visible_from_fn(ob, fn, total):
    show_window(ob, _runs([bool(fn(f)) for f in range(total)]), total)


def _runs(flags):
    runs, start = [], None
    for i, v in enumerate(flags + [False]):
        if v and start is None:
            start = i
        elif not v and start is not None:
            runs.append((start, i)); start = None
    return runs
