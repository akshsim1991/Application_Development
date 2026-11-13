#!/usr/bin/env bash
# Recent files, reading positions and preferences survive what the Windows app goes through: several windows (each a
# program of its own) closing in any order, another window's changes, Windows shutting down, names in any script, and an
# old settings file written in the 8-bit code page. Runs the real program under Wine and a virtual display.
# Usage: tests/persistence.sh [path/to/FeatherPDF.exe]   (needs wine, Xvfb, xdotool, python3 with reportlab)
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
closeAll() { for p in $(pids); do wine taskkill /PID "$p" >/dev/null 2>&1; sleep 3; done; }
inistr() { python3 -c "
import sys
raw=open('$ini','rb').read()
print(raw.decode('utf-16') if raw[:2]==b'\xff\xfe' else raw.decode('cp1252'))" 2>/dev/null; }

export DISPLAY=:93 LC_ALL=C.UTF-8
Xvfb :93 -screen 0 1400x900x24 >/dev/null 2>&1 & xvfb=$!
sleep 2
mkdir -p "$appdata"
[ -f "$ini" ] && cp "$ini" "$work/user-settings.ini"   # put back at the end
rm -rf "$appdata"/session-*
python3 - "$work" <<'PY'
import sys
from reportlab.pdfgen import canvas
for n in ['alpha','beta','gamma','ಕನ್ನಡ ಪ್ರಶ್ನೆ']:
    c=canvas.Canvas(f'{sys.argv[1]}/{n}.pdf')
    for i in range(6): c.drawString(72,700,f'{n} page {i+1}'); c.showPage()
    c.save()
PY

echo "an old settings file (8-bit code page) becomes UTF-16, its settings kept"
printf '[view]\r\ntheme=5\r\n[recent]\r\nfile0=C:\\old\\kept.pdf\r\nfile1=C:\\old\\?????.pdf\r\n' > "$ini"
(wine "$exe" "$(W "$work/gamma.pdf")" >/dev/null 2>&1 &); sleep 12
check "UTF-16 now"                       "[ \"\$(head -c 2 '$ini' | od -An -tx1 | tr -d ' ')\" = fffe ]"
check "the theme and the old entry kept" "inistr | grep -q '^theme=5' && inistr | grep -q 'kept.pdf'"
check "a damaged name (?????) dropped when the list is written" "! inistr | grep -q '?????'"

echo "two windows: neither one's recent files are lost, whichever closes last"
(wine "$exe" --newwindow "$(W "$work/alpha.pdf")" >/dev/null 2>&1 &); sleep 12
check "both in the list while open"      "inistr | grep -q 'alpha.pdf' && inistr | grep -q 'gamma.pdf'"
p2=$(pids | sed -n 2p); wine taskkill /PID "$p2" >/dev/null 2>&1; sleep 3
p1=$(pids | sed -n 1p); wine taskkill /PID "$p1" >/dev/null 2>&1; sleep 3
check "both still there after both closed" "inistr | grep -q 'alpha.pdf' && inistr | grep -q 'gamma.pdf'"

echo "another window's changes are not undone"
(wine "$exe" "$(W "$work/beta.pdf")" >/dev/null 2>&1 &); sleep 12
python3 - "$ini" <<'PY'
import sys
p=sys.argv[1]; t=open(p,'rb').read().decode('utf-16')
t=t.replace('theme=5','theme=3').replace('[recent]\r\n','[recent]\r\nfile0=C:\\other\\window.pdf\r\n',1)
lines=t.split('\r\n'); out=[]; sec=None; n=0
for l in lines:
    if l.startswith('['): sec=l; n=0
    elif sec=='[recent]' and l.startswith('file'): l='file%d=%s'%(n,l.split('=',1)[1]); n+=1
    out.append(l)
open(p,'wb').write(b'\xff\xfe'+'\r\n'.join(out).encode('utf-16-le'))
PY
closeAll
check "its theme and its recent file kept" "inistr | grep -q '^theme=3' && inistr | grep -q 'window.pdf'"
check "this window's file added"           "inistr | grep -q 'beta.pdf'"

echo "Windows shuts down: the place in the document kept"
(wine "$exe" "$(W "$work/ಕನ್ನಡ ಪ್ರಶ್ನೆ.pdf")" >/dev/null 2>&1 &); sleep 12
xdotool mousemove 600 400 click 1 key End; sleep 2
timeout 60 wine wineboot --end-session >/dev/null 2>&1; sleep 4
check "no Feather PDF left running"         "[ -z \"\$(pids)\" ]"
check "a Kannada name kept as it is"        "inistr | grep -q '^file0=.*ಕನ್ನಡ ಪ್ರಶ್ನೆ.pdf'"
check "its reading position saved (scrolled)" "inistr | grep -E '^p[0-9]+=.*ಕನ್ನಡ ಪ್ರಶ್ನೆ.pdf\\|[0-9.]+\\|[1-9][0-9]*\\|' >/dev/null"

closeAll
kill $xvfb 2>/dev/null
rm -rf "$appdata"/session-*
if [ -f "$work/user-settings.ini" ]; then cp "$work/user-settings.ini" "$ini"; else rm -f "$ini"; fi
rm -rf "$work"
[ $fail -eq 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
