#!/usr/bin/env bash
# Encode frames, mix narration + ducked background music, write output/dc_motor.mp4
set -euo pipefail
D="$(cd "$(dirname "$0")/.." && pwd)"; cd "$D/build"
ffmpeg -y -loglevel error -framerate 30 -i frames/f%05d.jpg -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p silent.mp4
ffmpeg -y -loglevel error -i silent.mp4 -i narration.wav -i music.wav -filter_complex \
"[1:a]aresample=48000,pan=stereo|c0=c0|c1=c0,asplit=2[n1][n2];[2:a]aresample=48000,volume=0.16[m];[m][n1]sidechaincompress=threshold=0.02:ratio=6:attack=30:release=700[md];[md][n2]amix=inputs=2:normalize=0,loudnorm=I=-16:TP=-1.5:LRA=11[a]" \
-map 0:v -map "[a]" -c:v copy -c:a aac -b:a 192k -movflags +faststart "$D/output/dc_motor.mp4"
ls -lh "$D/output/dc_motor.mp4"
