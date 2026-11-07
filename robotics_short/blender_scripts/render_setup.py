"""PREVIEW / FINAL render presets, device selection (GPU -> CPU fallback)."""
import bpy
from config import *    # noqa


def pick_device():
    """Enable the best available Cycles compute device; return its label."""
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for kind in ("OPTIX", "CUDA", "HIP", "METAL", "ONEAPI"):
            try:
                prefs.compute_device_type = kind
                prefs.get_devices()
            except Exception:
                continue
            devs = [d for d in prefs.devices if d.type == kind]
            if devs:
                for d in prefs.devices:
                    d.use = d.type == kind
                bpy.context.scene.cycles.device = "GPU"
                return f"GPU/{kind}"
    except Exception:
        pass
    bpy.context.scene.cycles.device = "CPU"
    return "CPU"


def eevee_ok():
    """Headless Linux servers usually have no GL context -> Eevee unusable."""
    return False if not bpy.app.background else False


def apply(preset_name):
    p = PRESETS[preset_name]
    sc = bpy.context.scene
    r = sc.render
    r.resolution_x, r.resolution_y, r.resolution_percentage = p["w"], p["h"], 100
    r.fps = FPS
    r.image_settings.file_format = "PNG"
    r.image_settings.color_mode = "RGB"
    r.image_settings.compression = 15
    r.use_motion_blur = p["motion_blur"]
    r.motion_blur_shutter = 0.3 if hasattr(r, "motion_blur_shutter") else 0.3
    r.film_transparent = False
    sc.render.engine = "CYCLES"          # Eevee needs a GPU/GL context; see README
    r.use_persistent_data = True
    c = sc.cycles
    c.samples = p["samples"]
    c.use_denoising = p["denoise"]
    c.denoiser = "OPENIMAGEDENOISE"
    if preset_name == "preview":
        c.denoising_quality, c.denoising_prefilter = "FAST", "FAST"
    c.use_adaptive_sampling = True
    c.adaptive_threshold = 0.03 if preset_name == "preview" else 0.008
    c.max_bounces = p["bounces"]
    c.diffuse_bounces, c.glossy_bounces = 2, min(3, p["bounces"])
    c.transmission_bounces, c.volume_bounces = 2, 0
    c.sample_clamp_indirect = 8.0
    c.caustics_reflective = c.caustics_refractive = False
    c.use_fast_gi = False
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    sc.view_settings.exposure = 0.0
    sc.render.threads_mode = "AUTO"
    dev = pick_device()
    cam = sc.camera
    if cam:
        cam.data.dof.use_dof = p["dof"]
    return dev
