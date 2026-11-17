#!/usr/bin/env bash
# File Explorer's "Convert to PDF and combine all the selected file(s)" and "Convert to PDF": File Explorer starts the
# program once per selected file, so this starts them all at once the same way, under Wine and a virtual display.
# Also: the right-click entries switched on and off (Settings > File Explorer), and the question after converting.
# Usage: tests/convert_combine.sh [path/to/FeatherPDF.exe]   (needs wine, Xvfb, xdotool, python3 with reportlab, pillow, pikepdf)
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"
exe="${1:-$root/build/FeatherPDF.exe}"
work="$(mktemp -d)"
appdata="$(winepath -u 'C:\users\root\AppData\Roaming' 2>/dev/null || echo /root/.wine/drive_c/users/root/AppData/Roaming)/FeatherPDF"
ini="$appdata/settings.ini"
fail=0
ok()  { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fail=1; }
check(){ if eval "$2"; then ok "$1"; else bad "$1"; fi; }
W() { echo "Z:$(echo "$1" | tr / '\\')"; }
pids() { wine tasklist 2>/dev/null | tr -d '\r' | awk '/FeatherPDF/{print $2}'; }
waitDone() { for i in $(seq 1 ${1:-60}); do [ -z "$(pids)" ] && return 0; sleep 1; done; return 1; }
setAfter() { python3 - "$ini" "$1" <<'PY'
import sys, re, os
p, v = sys.argv[1], sys.argv[2]
t = ''
if os.path.exists(p):
    raw = open(p, 'rb').read(); t = raw.decode('utf-16') if raw[:2] == b'\xff\xfe' else raw.decode('cp1252')
if 'afterConvert=' in t: t = re.sub(r'afterConvert=\d', 'afterConvert=' + v, t)
elif '[view]\r\n' in t: t = t.replace('[view]\r\n', '[view]\r\nafterConvert=' + v + '\r\n', 1)
else: t = '[view]\r\nafterConvert=' + v + '\r\n' + t
open(p, 'wb').write(b'\xff\xfe' + t.encode('utf-16-le'))
PY
}
outline() { python3 -c "
import pikepdf, sys
p = pikepdf.open(sys.argv[1])
with p.open_outline() as o: print(len(p.pages), '|'.join(i.title for i in o.root))" "$1" 2>/dev/null; }

export DISPLAY=:97 LC_ALL=C.UTF-8
Xvfb :97 -screen 0 1400x900x24 >/dev/null 2>&1 & xvfb=$!
sleep 2
mkdir -p "$appdata"
[ -f "$ini" ] && cp "$ini" "$work/user-settings.ini"
python3 - "$work" <<'PY'
import sys, os
from PIL import Image, ImageDraw
from reportlab.pdfgen import canvas
w = sys.argv[1]
os.makedirs(f'{w}/mixed'); os.makedirs(f'{w}/many'); os.makedirs(f'{w}/waves')
for n, c in [('img2.png', 'red'), ('img10.jpg', 'blue')]:
    im = Image.new('RGB', (800, 600), c); ImageDraw.Draw(im).text((50, 50), n, fill='white'); im.save(f'{w}/mixed/{n}')
open(f'{w}/mixed/notes.txt', 'w').write('Plain text notes\nSecond line\n')
open(f'{w}/mixed/readme.md', 'w').write('# Heading\n\nSome *Markdown* text.\n')
c = canvas.Canvas(f'{w}/mixed/report.pdf'); c.bookmarkPage('a'); c.addOutlineEntry('Report start', 'a', 0)
c.drawString(72, 700, 'Report page 1'); c.showPage(); c.drawString(72, 700, 'Report page 2'); c.showPage(); c.save()
open(f'{w}/mixed/broken.png', 'wb').write(b'\x89PNG\r\n\x1a\nnot really')
for i in range(1, 31):
    Image.new('RGB', (300, 200), (i * 8, 100, 200)).save(f'{w}/many/p{i}.png')
for i in range(1, 6):
    Image.new('RGB', (300, 200), (200, i * 40, 50)).save(f'{w}/waves/w{i}.png')
PY

echo "six files of five kinds and a damaged one, chosen together"
setAfter 3
for f in readme.md img10.jpg notes.txt report.pdf img2.png broken.png; do (wine "$exe" --convert-combine "$(W "$work/mixed/$f")" >/dev/null 2>&1 &); done
sleep 15
check "the damaged file is told about (it is not asked: told anyway)" "xdotool search --onlyvisible --name 'Convert to PDF' >/dev/null"
xdotool key Return; waitDone 30
check "one PDF, named after the first file in it" "[ \$(ls '$work/mixed' | grep -c combined) = 1 ] && [ -f '$work/mixed/img2 (combined).pdf' ]"
check "in the order of their names, a bookmark for each, the PDF's own inside" "[ \"\$(outline '$work/mixed/img2 (combined).pdf')\" = '6 img2.png|img10.jpg|notes.txt|readme.md|report.pdf' ]"

echo "thirty pictures at once"
for i in $(seq 1 30); do (wine "$exe" --convert-combine "$(W "$work/many/p$i.png")" >/dev/null 2>&1 &); done
waitDone 120
check "one PDF of 30 pages" "[ \$(ls '$work/many' | grep -c combined) = 1 ] && outline '$work/many/p1 (combined).pdf' | grep -q '^30 '"
check "p1, p2 ... p10 ... p30: in the order Explorer lists them" "outline '$work/many/p1 (combined).pdf' | grep -q '^30 p1.png|p2.png|p3.png|p4.png|p5.png|p6.png|p7.png|p8.png|p9.png|p10.png|p11.png'"

echo "files that come in two waves (a slow PC)"
for i in 1 2 3; do (wine "$exe" --convert-combine "$(W "$work/waves/w$i.png")" >/dev/null 2>&1 &); done
sleep 1.2
for i in 4 5; do (wine "$exe" --convert-combine "$(W "$work/waves/w$i.png")" >/dev/null 2>&1 &); done
waitDone 60
check "still one PDF with all five" "[ \$(ls '$work/waves' | grep -c combined) = 1 ] && outline '$work/waves/w1 (combined).pdf' | grep -q '^5 '"

echo "the right-click entries (Settings > File Explorer runs this with the right to change them)"
q() { wine reg query "HKLM\\Software\\Classes\\SystemFileAssociations\\$1\\shell\\$2\\command" >/dev/null 2>&1; }
wine "$exe" --explorer-menu convert=1 combine=0 >/dev/null 2>&1
check "Convert to PDF on, combine off" "q .docx FeatherPDF.ConvertToPDF && ! q .docx FeatherPDF.ConvertCombine && ! q .pdf FeatherPDF.ConvertCombine"
wine "$exe" --explorer-menu convert=0 combine=1 >/dev/null 2>&1
check "Convert to PDF off, combine on (PDFs too)" "! q .jpg FeatherPDF.ConvertToPDF && q .jpg FeatherPDF.ConvertCombine && q .pdf FeatherPDF.ConvertCombine"
check "combine is offered for any number of files" "wine reg query 'HKLM\\Software\\Classes\\SystemFileAssociations\\.png\\shell\\FeatherPDF.ConvertCombine' /v MultiSelectModel 2>/dev/null | grep -q Player"
wine "$exe" --explorer-menu convert=1 combine=1 >/dev/null 2>&1
check "both on again" "q .png FeatherPDF.ConvertToPDF && q .png FeatherPDF.ConvertCombine"

echo "the question after converting"
setAfter 0
(wine "$exe" --convert-open "$(W "$work/mixed/img10.jpg")" >/dev/null 2>&1 &)
sleep 10
check "it asks (Convert to PDF)" "xdotool search --onlyvisible --name 'Convert to PDF' >/dev/null"
check "the PDF is made before asking" "[ -f '$work/mixed/img10.pdf' ]"
dlg=$(xdotool search --onlyvisible --name 'Convert to PDF' | head -1)
# its Close button: at the bottom, under the first button (Wine draws the dialog as here; its Escape is not tried)
[ -n "$dlg" ] && { eval "$(xdotool getwindowgeometry --shell "$dlg")"; xdotool mousemove $((X + WIDTH * 56 / 100)) $((Y + HEIGHT - 16)) click 1; }
sleep 3
check "Close: nothing is opened" "[ -z \"\$(pids)\" ]"

for p in $(pids); do wine taskkill /F /PID "$p" >/dev/null 2>&1; done
kill $xvfb 2>/dev/null
if [ -f "$work/user-settings.ini" ]; then cp "$work/user-settings.ini" "$ini"; fi
rm -rf "$work"
[ $fail -eq 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
