"""Procedural materials (no external textures)."""
import bpy
from config import *   # noqa

_cache = {}


def _principled(name, base, metal=0.0, rough=0.5, coat=0.0, emit=None, emit_str=0.0, spec=0.5):
    if name in _cache:
        return _cache[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    for k, v in (("Coat Weight", coat), ("Coat Roughness", 0.08),
                 ("Specular IOR Level", spec)):
        if k in b.inputs:
            b.inputs[k].default_value = v
    if emit is not None:
        b.inputs["Emission Color"].default_value = (*emit, 1)
        b.inputs["Emission Strength"].default_value = emit_str
    m.diffuse_color = (*base, 1)
    _cache[name] = m
    return m


def _brushed(m, scale=220.0, strength=0.06):
    """Fine anisotropic-looking noise bump for brushed steel."""
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = scale
    tex.inputs["Detail"].default_value = 8
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.0, 0.02, 1.0)
    co = nt.nodes.new("ShaderNodeTexCoord")
    nt.links.new(co.outputs["Object"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], tex.inputs["Vector"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = strength
    nt.links.new(tex.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


def paint():       return _principled("RobotPaint", (0.80, 0.82, 0.85), 0.15, 0.28, coat=1.0)
def paint_orange():return _principled("AccentOrange", (0.95, 0.30, 0.03), 0.1, 0.3, coat=1.0)
def polymer():     return _principled("DarkPolymer", (0.025, 0.027, 0.03), 0.0, 0.42, spec=0.4)
def steel():       return _brushed(_principled("BrushedSteel", (0.62, 0.64, 0.66), 1.0, 0.33))
def part_metal():  return _brushed(_principled("PartAluminium", (0.80, 0.78, 0.74), 1.0, 0.22), 300, 0.04)
def floor():       return _principled("FloorGloss", (0.018, 0.019, 0.022), 0.0, 0.16, spec=0.6)
def wall():        return _principled("WallDark", (0.03, 0.032, 0.036), 0.0, 0.7)
def table():       return _principled("TableTop", (0.05, 0.055, 0.06), 0.6, 0.35)
def weld_bead():   return _principled("WeldBead", (0.5, 0.18, 0.05), 0.8, 0.4, emit=(1.0, 0.3, 0.05), emit_str=1.5)


def emissive(name, color, strength):
    """Pure emission shader (stays saturated, ignores scene lighting)."""
    key = "EM_" + name
    if key in _cache:
        return _cache[key]
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*color, 1)
    em.inputs["Strength"].default_value = strength
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])
    m.diffuse_color = (*color, 1)
    _cache[key] = m
    return m


def led(name, color, strength=4.0):
    return emissive("LED_" + name, color, strength * 0.5)


def cube_part():   return _principled("CubeBlue", (0.05, 0.18, 0.55), 0.3, 0.25, coat=1.0)
