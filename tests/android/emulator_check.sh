#!/usr/bin/env bash
# Installs the APK on a running emulator and runs the app's build check (SelfTest.kt): it opens a document from the
# app's folder, edits a paragraph, highlights, adds a note, draws with touches, finds, switches to night colours, undoes,
# redoes and saves, taking pictures of the screen. Then the saved file is checked from outside.
# Usage: tests/android/emulator_check.sh app.apk out-folder
set -uo pipefail
apk="$1"; out="$2"
root="$(cd "$(dirname "$0")/../.." && pwd)"
pkg=com.featherpdf.android
dir=/storage/emulated/0/Android/data/$pkg/files
mkdir -p "$out"
adb wait-for-device
adb install -r "$apk" || { echo "FAIL the APK does not install"; exit 1; }
adb shell am start -W -n $pkg/.MainActivity >/dev/null
sleep 4
adb exec-out screencap -p > "$out/00-start.png"
python3 "$root/tests/android/make_test_pdf.py" "$out/in.pdf"
adb shell mkdir -p "$dir"
adb push "$out/in.pdf" "$dir/in.pdf" >/dev/null
adb shell rm -rf "$dir/selftest"
adb logcat -c
adb shell am start -W -n $pkg/.ViewerActivity \
  --es com.featherpdf.path "$dir/in.pdf" --es com.featherpdf.selftest "$dir/selftest" \
  --es com.featherpdf.selftest.at "0 100 700" --es com.featherpdf.selftest.sweep "0 72 765 250 765" \
  --es com.featherpdf.selftest.append "Edited on Android." >/dev/null
done=0
for i in $(seq 1 90); do
  if adb shell cat "$dir/selftest/report.txt" 2>/dev/null | grep -q DONE; then done=1; break; fi
  [ "$i" = 8 ] && adb exec-out screencap -p > "$out/00-running.png"
  sleep 2
done
adb pull "$dir/selftest" "$out/" >/dev/null 2>&1
adb logcat -d > "$out/logcat.txt" 2>/dev/null
if [ $done = 0 ]; then
  echo "FAIL the build check did not finish"
  grep -E "AndroidRuntime|FATAL|featherpdf" "$out/logcat.txt" | tail -40
  exit 1
fi
cat "$out/selftest/report.txt"
fail=0
grep -q "^ALL PASSED" "$out/selftest/report.txt" || fail=1
python3 - "$out/selftest/saved.pdf" <<'PY' || fail=1
import sys, pikepdf
from pdfminer.high_level import extract_text
p = pikepdf.open(sys.argv[1])
kinds = sorted({str(a.Subtype) for page in p.pages for a in page.get("/Annots", [])})
text = extract_text(sys.argv[1])
ok = "Edited on Android." in text and {"/Highlight", "/Text", "/Ink"} <= set(kinds) and len(p.pages) == 3
print(("  ok   " if ok else "  FAIL ") + f"the saved file, read by other tools: {len(p.pages)} pages, annotations {kinds}, edit {'found' if 'Edited on Android.' in text else 'missing'}")
sys.exit(0 if ok else 1)
PY
if grep -q "FATAL EXCEPTION" "$out/logcat.txt"; then echo "  FAIL the app crashed"; grep -A20 "FATAL EXCEPTION" "$out/logcat.txt" | head -40; fail=1; fi
[ $fail = 0 ] && echo "EMULATOR CHECK PASSED" || { echo "EMULATOR CHECK FAILED"; exit 1; }
