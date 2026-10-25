#!/usr/bin/env bash
# Smoke test of the command line features, run under Wine with the cross-compiled build.
# Needs: wine, poppler-utils (pdftotext, pdfimages, pdfsig, pdfinfo), python3 with reportlab pillow pikepdf python-docx openpyxl,
#        and openssl (for the test certificate). Usage: tests/smoke.sh [path/to/FeatherPDF.exe]
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"
exe="${1:-$root/build/FeatherPDF.exe}"
work="$(mktemp -d)"
export WINEDEBUG=-all
fail=0
ok()   { echo "  ok   $1"; }
bad()  { echo "  FAIL $1"; fail=1; }
check(){ if eval "$2"; then ok "$1"; else bad "$1"; fi; }
win()  { winepath -w "$1" 2>/dev/null; }
run()  { wine "$exe" "$@"; }

python3 "$root/tests/make_samples.py" "$work" >/dev/null
openssl req -x509 -newkey rsa:2048 -keyout "$work/k.pem" -out "$work/c.pem" -days 30 -nodes -subj "/CN=Smoke Test/O=Feather" 2>/dev/null
openssl pkcs12 -export -out "$work/t.pfx" -inkey "$work/k.pem" -in "$work/c.pem" -passout pass:pw -legacy 2>/dev/null
W() { win "$work/$1"; }

echo "export"
for f in txt md docx xlsx csv png jpg; do run --export $f "$(W rich.pdf)" "$(W out.$f)" 100; check "export $f" "[ -s '$work/out.$f' ] || [ -s '$work/out-1.$f' ]"; done
check "text has the heading"        "grep -q 'Quarterly Report' '$work/out.txt'"
check "markdown has a table"        "grep -q '| Item | Qty | Price |' '$work/out.md'"
check "csv keeps the number"        "grep -q '\"2,250.75\"' '$work/out.csv'"
check "docx opens (python-docx)"    "python3 -c \"import docx;d=docx.Document('$work/out.docx');assert len(d.tables)==1\""
check "xlsx opens (openpyxl)"       "python3 -c \"import openpyxl;w=openpyxl.load_workbook('$work/out.xlsx');assert w.active['C3'].value==2250.75\""

echo "compress"
run --compress "$(W img.pdf)" "$(W img_c.pdf)" 150
check "smaller file"                "[ \$(stat -c %s '$work/img_c.pdf') -lt \$(stat -c %s '$work/img.pdf') ]"
check "text kept"                   "pdftotext '$work/img_c.pdf' - | grep -q 'Picture test'"

echo "password protection"
run --protect "$(W rich.pdf)" "$(W prot.pdf)" open123 owner456
check "needs the password"          "! pdfinfo '$work/prot.pdf' >/dev/null 2>&1"
check "opens with the password"     "pdftotext -upw open123 '$work/prot.pdf' - | grep -q 'Quarterly Report'"
check "AES-256 (pikepdf)"           "python3 -c \"import pikepdf;p=pikepdf.open('$work/prot.pdf',password='owner456');assert p.encryption.R==6\""

echo "OCR"
run --ocr "$(W scan.pdf)" "$(W scan_ocr.pdf)" eng; rc=$?
if [ $rc -eq 5 ]; then echo "  skip OCR engine not built"; else check "words recognised" "pdftotext '$work/scan_ocr.pdf' - | grep -qi 'supplier shall deliver'"; fi

echo "signatures"
run --sign "$(W rich.pdf)" "$(W signed.pdf)" "$(W t.pfx)" pw
check "pdfsig: signature valid"     "pdfsig '$work/signed.pdf' | grep -q 'Signature is Valid'"
run --checksig "$(W signed.pdf)" "$(W sig.txt)"
check "own check: valid"            "grep -q '^VALID' '$work/sig.txt'"
python3 - "$work" <<'PY'
import sys; w=sys.argv[1]; d=bytearray(open(w+'/signed.pdf','rb').read()); d[60]^=1; open(w+'/tampered.pdf','wb').write(d)
PY
run --checksig "$(W tampered.pdf)" "$(W sig2.txt)"
check "own check: tampering found"  "grep -q '^CHANGED' '$work/sig2.txt'"

echo "search all PDFs in a folder"
mkdir -p "$work/lib"; cp "$work/rich.pdf" "$work/img.pdf" "$work/lib/"
run --search "$(W lib)" "Quarterly" "$(W srch.txt)"
check "finds the phrase"            "grep -q 'rich.pdf|1|' '$work/srch.txt'"
run --search "$(W lib)" "Quarterly" "$(W srch2.txt)"
check "second search uses the saved index" "grep -q 'scanned=0' '$work/srch2.txt'"

echo "redaction and its check"
run --redact "$(W rich.pdf)" "$(W red.pdf)" "Quarterly"; rc=$?
check "redaction verified (exit code)" "[ $rc -eq 0 ]"
check "the text is gone"            "! pdftotext '$work/red.pdf' - | grep -q 'Quarterly'"

echo "batch"
mkdir -p "$work/bin"; cp "$work/rich.pdf" "$work/bin/"
printf '[batch]\ninput=%s\noutput=%s\nwatermark=DRAFT\ncompress=1\nuserPassword=abc\nownerPassword=xyz\n' "$(W bin)" "$(W bout)" > "$work/job.ini"
run --batch "$(W job.ini)"
check "batch result is encrypted"   "python3 -c \"import pikepdf;p=pikepdf.open('$work/bout/rich.pdf',password='abc');assert p.is_encrypted and len(p.pages)>=1\""

echo "forms"
python3 - "$work" <<'PY'
import sys
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
c=canvas.Canvas(sys.argv[1]+'/flat.pdf',pagesize=A4); c.setFont('Helvetica',12)
c.drawString(72,730,'Name: ____________________________'); c.drawString(72,690,'Email:'); c.line(120,688,380,688)
c.rect(72,640,10,10); c.drawString(90,641,'I agree'); c.save()
PY
run --formdetect "$(W flat.pdf)" "$(W flat_f.pdf)"
check "fields created"              "python3 -c \"import pikepdf;p=pikepdf.open('$work/flat_f.pdf');assert len(p.Root.AcroForm.Fields)==3\""
printf 'Name,Email,I agree\nAnna,a@x.org,yes\n' > "$work/people.csv"
run --formfill "$(W flat_f.pdf)" "$(W people.csv)" "$(W filled)"
check "values filled in"            "python3 -c \"import pikepdf;p=pikepdf.open('$work/filled/row1.pdf');v={str(f.T):str(f.get('/V')) for f in p.Root.AcroForm.Fields};assert v['Name']=='Anna' and v['I agree']=='/Yes',v\""
run --formexport "$(W filled/row1.pdf)" "$(W ans.csv)"
check "answers exported"            "grep -q 'Anna' '$work/ans.csv'"

echo "visual compare"
run --visualdiff "$(W rich.pdf)" "$(W red.pdf)" "$(W vd.png)" 1
check "overlay written"             "[ -s '$work/vd.png' ] && grep -q 'ok=1' '$work/vd.png.txt'"

if command -v openssl >/dev/null && python3 -c 'import http.server' 2>/dev/null; then
echo "time stamp and visible signature (local test server)"
tsa="$work/tsa"; mkdir -p "$tsa"; ( cd "$tsa" &&
  openssl req -x509 -newkey rsa:2048 -nodes -keyout tsa.key -out tsa.crt -days 30 -subj "/CN=Local Test TSA" -addext "extendedKeyUsage=critical,timeStamping" 2>/dev/null &&
  echo 01 > serial &&
  printf '[tsa]\ndefault_tsa=t\n[t]\ndir=.\nserial=./serial\ncrypto_device=builtin\nsigner_cert=./tsa.crt\ncerts=./tsa.crt\nsigner_key=./tsa.key\nsigner_digest=sha256\ndefault_policy=1.2.3.4.1\ndigests=sha256\naccuracy=secs:1\nordering=yes\ntsa_name=yes\ness_cert_id_chain=no\ness_cert_id_alg=sha256\n' > ts.cnf &&
  cat > server.py <<'PY'
import http.server, subprocess
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        n=int(self.headers['Content-Length']); open('q.tsq','wb').write(self.rfile.read(n))
        subprocess.run(['openssl','ts','-reply','-queryfile','q.tsq','-config','ts.cnf','-section','t','-out','r.tsr'],check=True,capture_output=True)
        d=open('r.tsr','rb').read()
        self.send_response(200); self.send_header('Content-Type','application/timestamp-reply'); self.send_header('Content-Length',str(len(d))); self.end_headers(); self.wfile.write(d)
    def log_message(self,*a): pass
http.server.HTTPServer(('127.0.0.1',8319),H).serve_forever()
PY
  ( setsid python3 server.py >/dev/null 2>&1 & echo $! > pid ) )
sleep 1.5
run --sign "$(W rich.pdf)" "$(W ts.pdf)" "$(W t.pfx)" pw http://127.0.0.1:8319 visible
check "still a valid signature"     "pdfsig '$work/ts.pdf' | grep -q 'Signature is Valid'"
check "time stamp inside the signature" "python3 -c \"
from pyhanko.pdf_utils.reader import PdfFileReader
import io,hashlib
r=PdfFileReader(io.BytesIO(open('$work/ts.pdf','rb').read())); si=r.embedded_signatures[0].signer_info
a=[x for x in si['unsigned_attrs'] if x['type'].native=='signature_time_stamp_token'][0]
info=a['values'][0]['content']['encap_content_info']['content'].parsed
assert hashlib.sha256(si['signature'].native).digest()==info['message_imprint']['hashed_message'].native\""
check "visible stamp drawn"         "pdftoppm -r 40 -png '$work/ts.pdf' '$work/tsimg' && python3 -c \"
from PIL import Image
im=Image.open('$work/tsimg-1.png').convert('RGB'); w,h=im.size
assert any(im.getpixel((x,y))[2]>240 and im.getpixel((x,y))[0]<250 and im.getpixel((x,y))[0]>230 for x in range(w//2,w) for y in range(h//2,h))\""
kill "$(cat "$tsa/pid")" 2>/dev/null
else echo "  skip time stamp test (needs openssl and python3)"; fi

echo "text and Markdown to PDF"
printf '# Shopping list\n\n- **Milk** and bread\n- Eggs\n\n| Item | Qty |\n|---|---|\n| Tea | 2 |\n' > "$work/list.md"
run --topdf "$(W list.md)" "$(W list.pdf)"
check "heading and table text"      "pdftotext '$work/list.pdf' - | grep -q 'Shopping list' && pdftotext -layout '$work/list.pdf' - | grep -q 'Tea'"

echo "sanitise"
python3 -c "
import pikepdf
p=pikepdf.open('$work/rich.pdf'); p.docinfo['/Author']='Secret Person'
p.attachments['payroll.txt']=pikepdf.AttachedFileSpec(p,b'salaries')
p.save('$work/dirty.pdf')"
run --sanitize "$(W dirty.pdf)" "$(W clean.pdf)"
check "author and attachment gone"  "! grep -aq -e 'Secret Person' -e salaries '$work/clean.pdf' && python3 -c \"import pikepdf;assert len(pikepdf.open('$work/clean.pdf').attachments)==0\""
check "report written"              "[ -s '$work/clean.pdf.txt' ]"
check "text kept"                   "pdftotext '$work/clean.pdf' - | grep -q 'Quarterly Report'"

echo "page tools"
python3 -c "
import pikepdf
p=pikepdf.open('$work/rich.pdf'); p.pages.append(p.pages[0]); p.save('$work/twice.pdf')"
run --pages "$(W twice.pdf)" "$(W once.pdf)" nodupes
check "duplicate page removed"      "[ \$(pdfinfo '$work/once.pdf' | awk '/^Pages/{print \$2}') -eq 1 ]"
run --pages "$(W twice.pdf)" "$(W hf.pdf)" header
check "header and page numbers"     "pdftotext '$work/hf.pdf' - | grep -q 'Page 2 of 2'"

echo "form fields: radio, list, date, reset, flatten"
run --formtest "$(W rich.pdf)" "$(W ft)"
check "all kinds filled"            "grep -q 'size=Medium\[x\].*colour=Green when=25.10.2026 who=Ann agree=Yes\[x\]' '$work/ft.txt'"
check "reset clears them"           "grep -q '^reset: size=Off size=Off size=Off colour= when= who= agree=Off' '$work/ft.txt'"
check "flatten leaves no fields"    "grep -q 'fields after flatten: 0' '$work/ft.txt' && pdftotext '$work/ft-flat.pdf' - | grep -q 'Medium'"

echo "picture tools"
python3 -c "
from PIL import Image, ImageDraw
im=Image.new('RGB',(300,300),'white'); d=ImageDraw.Draw(im); d.rectangle((20,20,280,280),fill='green'); im.save('$work/sq.png')"
run --pictest "$(W img.pdf)" "$(W pt)" "$(W sq.png)"
check "all picture tools ran"       "[ \$(grep -c ' ok ' '$work/pt.txt') -eq 11 ]"
check "turned picture is upright box" "python3 -c \"
import re;t=open('$work/pt.txt').read()
o=[float(x) for x in re.search(r'original ok index=\d+ box=([-\d,]+)',t).group(1).split(',')]
r=[float(x) for x in re.search(r'rotate ok index=\d+ box=([-\d,]+)',t).group(1).split(',')]
assert abs((o[2]-o[0])-(r[3]-r[1]))<2 and abs((o[3]-o[1])-(r[2]-r[0]))<2\""
check "extracted picture"           "python3 -c \"from PIL import Image;assert Image.open('$work/pt-extract.png').size[0]>1000\""

echo "print layout (as the preview shows it)"
python3 -c "
import pikepdf
p=pikepdf.open('$work/img.pdf')
for i in range(4): p.pages.append(p.pages[0])
p.save('$work/five.pdf')"
run --printsheet "$(W five.pdf)" "$(W sheet.png)" 4 grey
check "4 per sheet: 2 sheets for 5 pages" "grep -q 'sheets=2 ' '$work/sheet.png.txt'"
check "greyscale sheet"             "python3 -c \"
from PIL import Image
im=Image.open('$work/sheet.png').convert('RGB')
assert all(max(p)-min(p)<=2 for p in im.get_flattened_data()) and min(min(p) for p in im.get_flattened_data())<128\""

echo "PDF/A-2b copy"
python3 -c "
import pikepdf
p=pikepdf.open('$work/rich.pdf'); p.pages.append(p.pages[0])
link=p.make_indirect(pikepdf.Dictionary(Type=pikepdf.Name.Annot,Subtype=pikepdf.Name.Link,Rect=[72,700,300,720],Dest=[p.pages[1].obj,pikepdf.Name.Fit]))
p.pages[0].obj['/Annots']=pikepdf.Array([link])
p.attachments['a.txt']=pikepdf.AttachedFileSpec(p,b'x')
p.Root.OpenAction=pikepdf.Dictionary(S=pikepdf.Name.JavaScript,JS='app.alert(1)')
p.save('$work/arch.pdf')"
run --pdfa "$(W arch.pdf)" "$(W arch-a.pdf)"
check "converted"                   "grep -q '^ok=1' '$work/arch-a.pdf.txt'"
check "names PDF/A-2b, sRGB intent" "python3 -c \"
import pikepdf;p=pikepdf.open('$work/arch-a.pdf');m=p.Root.Metadata.read_bytes()
assert b'<pdfaid:part>2</pdfaid:part>' in m and b'<pdfaid:conformance>B</pdfaid:conformance>' in m
oi=p.Root.OutputIntents[0];assert oi.S=='/GTS_PDFA1' and oi.DestOutputProfile.N==3 and '/ID' in p.trailer\""
check "no script, attachment"       "python3 -c \"
import pikepdf;p=pikepdf.open('$work/arch-a.pdf');assert '/OpenAction' not in p.Root and len(p.attachments)==0\""
check "link kept, printable"        "python3 -c \"
import pikepdf;p=pikepdf.open('$work/arch-a.pdf');a=p.pages[0].Annots[0];assert a.Subtype=='/Link' and a.F==4 and a.Dest[0].objgen==p.pages[1].objgen\""
check "visible fonts embedded"      "grep -q 'pictures=0' '$work/arch-a.pdf.txt' && ! pdffonts '$work/arch-a.pdf' | tail -n +3 | awk '{print \$(NF-4)}' | grep -q no || grep -q 'pictures=[1-9]' '$work/arch-a.pdf.txt'"
check "text still searchable"       "pdftotext '$work/arch-a.pdf' - | grep -q 'Quarterly Report'"
if [ -n "${VERAPDF:-}" ]; then
  check "veraPDF: PDF/A-2b passes"  "'$VERAPDF' --flavour 2b --format text '$work/arch-a.pdf' 2>/dev/null | grep -q '^PASS'"
else echo "  skip veraPDF check (set VERAPDF to the veraPDF command line)"; fi

echo "comment shapes"
run --annottest "$(W rich.pdf)" "$(W shapes.pdf)"
check "polygon, cloud and callout"  "python3 -c \"import pikepdf;p=pikepdf.open('$work/shapes.pdf');assert len(p.pages[0].Annots)==3\""


rm -rf "$work"
[ $fail -eq 0 ] && echo "ALL PASSED" || echo "SOME FAILED"
exit $fail
