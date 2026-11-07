"""bpy-side QA: (a) animation == kinematics, (b) mesh intersections, (c) camera framing.

    python blender_scripts/qa_scene.py  -> documentation/qa_scene.json
"""
import json
import os
import sys
import itertools
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import numpy as np
from mathutils.bvhtree import BVHTree
from bpy_extras.object_utils import world_to_camera_view
from config import *    # noqa
import motion


def main():
    bpy.ops.wm.open_mainfile(filepath=path("scenes", "robotics_short.blend"))
    sc = bpy.context.scene
    tool, cam = bpy.data.objects["tool_frame"], bpy.data.objects["Camera"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH" and "link" in o]
    rep = dict(pose_error=0.0, intersections=[], framing=[])
    frames = sorted(set(list(range(0, TOTAL_FRAMES, 30)) + [S2 + 1, 300, 420, 540, 700, 900, 1000, 1100, 1300,
                                                            1349, 1350, 1500, 1535, 1559, 1650, 1700, 1725, 1799]))
    seen = set()
    for f in frames:
        sc.frame_set(f)
        dg = bpy.context.evaluated_depsgraph_get()
        # (a) animation reproduces commanded pose
        M = np.array(tool.matrix_world)
        e = max(np.abs(M[:3, :3] - motion.orientation(f)).max(), np.abs(M[:3, 3] - motion.position(f)).max())
        rep["pose_error"] = max(rep["pose_error"], float(e))
        # (b) intersections
        vis = [o for o in meshes if not o.hide_render]
        bv = {}
        for o in vis:
            ev = o.evaluated_get(dg)
            me = ev.to_mesh()
            me.transform(ev.matrix_world)
            bv[o.name] = BVHTree.FromPolygons([v.co[:] for v in me.vertices], [p.vertices[:] for p in me.polygons])
            ev.to_mesh_clear()
        for a, b in itertools.combinations(vis, 2):
            la, lb = a["link"], b["link"]
            robot = lambda l: l <= 7
            if robot(la) and robot(lb) and abs(la - lb) < 2:
                continue                                  # adjacent links / tool on flange
            if {la, lb} == {7, 8} or (la == lb) or (la >= 20 and lb >= 20):
                continue                                  # tool holds part / static env
            if a.parent is b or b.parent is a:
                continue
            if la == 8 and lb == 8:
                continue
            if bv[a.name].overlap(bv[b.name]):
                key = (a.name, b.name)
                if key not in seen:
                    seen.add(key)
                    rep["intersections"].append(dict(frame=f, a=a.name, b=b.name))
        # (c) framing: TCP position in the camera view
        p = world_to_camera_view(sc, cam, tool.matrix_world.translation)
        rep["framing"].append(dict(frame=f, x=round(p.x, 3), y_from_top=round(1 - p.y, 3), in_front=p.z > 0))
    rep["frames_checked"] = len(frames)
    json.dump(rep, open(path("documentation", "qa_scene.json"), "w"), indent=1)
    print("pose_error", rep["pose_error"], "| intersections", len(rep["intersections"]))
    for i in rep["intersections"][:40]:
        print("  ", i)


if __name__ == "__main__":
    main()
