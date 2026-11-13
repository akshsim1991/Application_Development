#!/usr/bin/env bash
# Smoke test under Wine + Xvfb (NOT real Windows). Seeds synthetic Run entries, launches the exe, saves screenshots to $OUT.
# Needs: wine, xvfb, xdotool, imagemagick. Usage: tests/smoke-wine.sh [out_dir]
set -euo pipefail
cd "$(dirname "$0")/.."; OUT="${1:-/tmp/fsu-smoke}"; mkdir -p "$OUT"
export WINEPREFIX=/tmp/fsu-wp WINEDEBUG=-all
timeout 200 xvfb-run -a -s "-screen 0 1500x900x24" bash -c '
R="HKCU\Software\Microsoft\Windows\CurrentVersion\Run"
wine reg add "$R" /v "Test App" /d "\"C:\\windows\\system32\\notepad.exe\" /min" /f >/dev/null
wine reg add "$R" /v "Missing One" /d "\"C:\\nope\\gone.exe\" -x" /f >/dev/null
wine build/FeatherSystemUtilities.exe --startup & sleep 25
import -window root '"$OUT"'/startup.png
xdotool mousemove 200 100 click 1; xdotool key ctrl+a; sleep 1; xdotool mousemove 308 49 click 1; sleep 2; import -window root '"$OUT"'/multi-disable-confirm.png'
echo "screenshots in $OUT"
