#!/usr/bin/env bash
# The Android app's engine bridge (Android/app/src/main/cpp, Engine.java) built for this desktop and driven from Java with
# the system services given as on Android. Needs: clang++, cmake, ninja, a JDK, python3 with reportlab and pikepdf, and
# PDFium for Linux (scripts/fetch-pdfium.sh linux).
# Usage: tests/android/bridge_check.sh
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
build="$work/build"
cmake -S "$root/Android/app/src/main/cpp" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ >/dev/null
ninja -C "$build" >/dev/null
mkdir -p "$work/classes"
javac -d "$work/classes" "$root/Android/app/src/main/java/com/featherpdf/engine/Engine.java" "$root/tests/android/BridgeCheck.java"
python3 - "$work" <<'PY'
import sys
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
import pikepdf
w = sys.argv[1]
for name in ("doc", "plain"):
    c = canvas.Canvas(f"{w}/{name}.pdf", pagesize=A4)
    c.setFont("Helvetica-Bold", 18); c.drawString(72, 760, "Quarterly Report")
    c.setFont("Helvetica", 12)
    c.drawString(72, 700, "Sales rose in the third quarter, and costs stayed where they were.")
    c.drawString(72, 685, "The new shop opened in March and did well from its first week.")
    c.setFillColorRGB(1, 0, 0); c.rect(72, 72, 100, 50, fill=1, stroke=0); c.setFillColorRGB(0, 0, 0)
    c.showPage()
    for i in (2, 3):
        c.setFont("Helvetica", 12); c.drawString(72, 700, f"Page {i} of the report."); c.showPage()
    c.save()
p = pikepdf.open(f"{w}/plain.pdf")
p.save(f"{w}/locked.pdf", encryption=pikepdf.Encryption(user="s3cret", owner="owner-pw", R=6))
PY
FEATHER_PDFIUM="$root/third_party/pdfium-linux/lib/libpdfium.so" \
  java -Djava.library.path="$build" -cp "$work/classes" BridgeCheck "$work"
