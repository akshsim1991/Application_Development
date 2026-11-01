#!/usr/bin/env bash
# Installs the Kokoro neural TTS (ONNX build) and downloads its model files from GitHub.
set -euo pipefail
DIR="${KOKORO_DIR:-/tmp/kokoro}"
mkdir -p "$DIR" && cd "$DIR"
python3 -m venv venv
./venv/bin/pip install -q kokoro-onnx soundfile numpy
BASE=https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0
for f in kokoro-v1.0.onnx voices-v1.0.bin; do [ -s "$f" ] || curl -sSL -o "$f" "$BASE/$f"; done
echo "Kokoro ready in $DIR"
