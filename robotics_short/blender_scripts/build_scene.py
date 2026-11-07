"""Assemble the full scene and save scenes/robotics_short.blend.

Run (any of):
    python blender_scripts/build_scene.py [--preset preview|final]
    blender -b -P blender_scripts/build_scene.py -- --preset final
"""
import os
import sys
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from config import *    # noqa
import bl_robot, bl_environment, bl_lighting, bl_camera, bl_axes, render_setup


def parse():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="preview", choices=list(PRESETS))
    ap.add_argument("--out", default=path("scenes", "robotics_short.blend"))
    return ap.parse_args(argv)


def build(preset="preview", out=None):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.frame_start, sc.frame_end = 0, TOTAL_FRAMES - 1
    sc.render.fps = FPS
    sc.unit_settings.system = "METRIC"
    R = bl_robot.build()
    E = bl_environment.build()
    bl_lighting.build(preset)
    cam, tgt = bl_camera.build()
    bl_axes.build(cam, R)
    bl_robot.animate(R)
    bl_environment.animate(E)
    bl_camera.animate(cam, tgt)
    render_setup.apply(preset)
    out = out or path("scenes", "robotics_short.blend")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=out)
    return out


if __name__ == "__main__":
    a = parse()
    print("saved", build(a.preset, a.out))
