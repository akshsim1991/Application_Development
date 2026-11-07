"""Softbox key, rim lights and coloured accents + dark world."""
import math
import bpy
from mathutils import Vector
from bl_common import *


def _aim(light, target):
    c = light.constraints.new("TRACK_TO")
    c.target, c.track_axis, c.up_axis = target, "TRACK_NEGATIVE_Z", "UP_Y"


def area(name, loc, size, power, color, target, shape="RECTANGLE", size_y=None, spread=None):
    d = bpy.data.lights.new(name, "AREA")
    d.shape, d.size, d.size_y = shape, size, size_y or size
    d.energy, d.color = power, color
    if spread:
        d.spread = math.radians(spread)
    ob = bpy.data.objects.new(name, d)
    ob.location = loc
    link(ob)
    _aim(ob, target)
    return ob


def build(preset):
    sc = bpy.context.scene
    w = bpy.data.worlds.new("World")
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.010, 0.011, 0.014, 1)
    bg.inputs["Strength"].default_value = 1.0
    aim = empty("light_aim", loc=(0.7, 0.0, 0.7))
    L = {"aim": aim}
    # large softbox key (front-left of the robot cell)
    L["key"] = area("key_softbox", (3.2, -3.4, 3.4), 3.0, 330, (1.0, 0.97, 0.93), aim, size_y=2.2)
    # opposite soft fill
    L["fill"] = area("fill_panel", (-1.8, -3.2, 1.8), 2.4, 40, (0.8, 0.88, 1.0), aim)
    # rim lights from behind
    L["rim_cool"] = area("rim_cool", (2.6, 3.0, 2.2), 1.2, 330, (0.55, 0.75, 1.0), aim, size_y=2.6)
    L["rim_warm"] = area("rim_warm", (-2.4, 2.6, 1.9), 1.0, 230, (1.0, 0.55, 0.22), aim, size_y=2.4)
    # low coloured accent wash
    L["accent_blue"] = area("accent_blue", (2.4, 0.0, 0.25), 2.0, 30, (0.15, 0.4, 1.0), aim, size_y=0.3)
    L["accent_orange"] = area("accent_orange", (-0.8, -2.2, 0.25), 2.0, 22, (1.0, 0.45, 0.1), aim, size_y=0.3)
    # overhead light over the table for close-ups
    L["top"] = area("table_top_light", (0.9, 0.0, 2.4), 1.4, 90, (1.0, 0.97, 0.92), aim, shape="SQUARE")
    return L
