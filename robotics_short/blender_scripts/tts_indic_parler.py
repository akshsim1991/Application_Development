"""Generate Indian-English (en-IN) narration with ai4bharat/indic-parler-tts (free, Apache-2.0, local).

NOT executed in the build sandbox (huggingface.co is blocked there), so voice quality is
UNVERIFIED until you run the sample on your machine.

Setup (once):
    pip install torch soundfile transformers
    pip install git+https://github.com/huggingface/parler-tts.git

Step 1 - ~10 s sample for approval (first block + first sentence of block 2, verbatim):
    python blender_scripts/tts_indic_parler.py --sample
    python blender_scripts/tts_indic_parler.py --sample --audition     # several speakers
Step 2 - after approval, full narration (each block rendered separately, placed in its window):
    python blender_scripts/tts_indic_parler.py --full --speaker <name>
    python blender_scripts/audio_sync.py audio/narration.wav
"""
import argparse
import os
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import *    # noqa

MODEL = "ai4bharat/indic-parler-tts"
SPEAKERS = ["Aakash", "Jatin", "Dinesh", "Raghav", "Thoma"]      # audition; names per model card
DESC = ("{spk} speaks in a calm, clear, professional Indian English accent, like an educational narrator. "
        "Moderate speed, natural intonation, minimal emotion, very clear pronunciation, "
        "studio-quality recording with no background noise.")


def load():
    import torch
    from parler_tts import ParlerTTSForConditionalGeneration
    from transformers import AutoTokenizer
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    model = ParlerTTSForConditionalGeneration.from_pretrained(MODEL).to(dev)
    tok = AutoTokenizer.from_pretrained(MODEL)
    dtok = AutoTokenizer.from_pretrained(model.config.text_encoder._name_or_path)
    return model, tok, dtok, dev


def synth(bundle, text, speaker, out):
    import soundfile as sf
    model, tok, dtok, dev = bundle
    d = dtok(DESC.format(spk=speaker), return_tensors="pt").to(dev)
    p = tok(text, return_tensors="pt").to(dev)
    g = model.generate(input_ids=d.input_ids, attention_mask=d.attention_mask,
                       prompt_input_ids=p.input_ids, prompt_attention_mask=p.attention_mask)
    sf.write(out, g.cpu().numpy().squeeze(), model.config.sampling_rate)


def to48k_stereo(src, dst, pad_to=None, delay=0.0):
    af = f"adelay={int(delay*1000)}|{int(delay*1000)}," + (f"apad=whole_dur={pad_to}" if pad_to else "anull")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-ar", "48000", "-ac", "2", "-af", af, dst], check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", action="store_true")
    ap.add_argument("--audition", action="store_true")
    ap.add_argument("--full", action="store_true")
    ap.add_argument("--speaker", default=SPEAKERS[0])
    a = ap.parse_args()
    os.makedirs(path("audio"), exist_ok=True)
    bundle = load()
    if a.sample:
        first = NARRATION[1][2].split(". ")[0] + "."
        text = NARRATION[0][2] + " " + first
        for spk in (SPEAKERS if a.audition else [a.speaker]):
            raw = path("audio", f"sample_{spk}_raw.wav")
            synth(bundle, text, spk, raw)
            to48k_stereo(raw, path("audio", f"sample_{spk}.wav"))
            print("wrote", path("audio", f"sample_{spk}.wav"))
        return
    if a.full:
        import tempfile
        parts = []
        for i, (t0, t1, text) in enumerate(NARRATION):
            raw = path("audio", f"block{i+1}_raw.wav")
            synth(bundle, text, a.speaker, raw)
            seg = path("audio", f"block{i+1}.wav")
            to48k_stereo(raw, seg, pad_to=t1 - t0)
            parts.append(seg)
        lst = path("audio", "blocks.txt")
        open(lst, "w").write("".join(f"file '{os.path.basename(p)}'\n" for p in parts))
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", lst,
                        "-t", str(DURATION_S), "-ar", "48000", "-ac", "2", path("audio", "narration.wav")], check=True)
        print("wrote", path("audio", "narration.wav"))


if __name__ == "__main__":
    main()
