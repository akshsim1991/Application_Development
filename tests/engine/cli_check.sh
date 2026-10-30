#!/usr/bin/env bash
# The command line features checked with any build (the Mac app, the Linux engine check), as tests/smoke.sh checks them
# on Windows: export, compress, passwords, redaction, forms, compare, text to PDF, hidden data, page tools, pictures,
# PDF/A, fonts and comment shapes. Results are checked with independent tools (pdfminer for text, pikepdf for structure,
# python-docx and openpyxl). Needs python3 with: reportlab pillow pikepdf python-docx openpyxl pdfminer.six
# Usage: cli_check.sh PROGRAM [jpeg]   ("jpeg": the build can write JPEG pictures; the Linux check cannot)
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
prog="$1"
jpeg="${2:-}"
work="$(mktemp -d)"
fail=0
ok()   { echo "  ok   $1"; }
bad()  { echo "  FAIL $1"; fail=1; }
check(){ if eval "$2"; then ok "$1"; else bad "$1"; fi; }
run()  { "$prog" "$@" </dev/null >/dev/null 2>&1; }
mkdir -p "${TMPDIR:-/tmp}" >/dev/null 2>&1
text() { python3 -c "import sys;from pdfminer.high_level import extract_text;print(extract_text(sys.argv[1]))" "$1" 2>/dev/null; }
pages(){ python3 -c "import sys,pikepdf;print(len(pikepdf.open(sys.argv[1]).pages))" "$1"; }

python3 "$root/tests/make_samples.py" "$work" >/dev/null 2>&1
W="$work"

echo "export"
for f in txt md docx xlsx csv png; do run --export $f "$W/rich.pdf" "$W/out.$f" 100; check "export $f" "[ -s '$W/out.$f' ] || [ -s '$W/out-1.$f' ]"; done
check "text has the heading"        "grep -q 'Quarterly Report' '$W/out.txt'"
check "markdown has a table"        "grep -q '| Item | Qty | Price |' '$W/out.md'"
check "csv keeps the number"        "grep -q '\"2,250.75\"' '$W/out.csv'"
check "docx opens (python-docx)"    "python3 -c \"import docx;d=docx.Document('$W/out.docx');assert len(d.tables)==1\""
check "xlsx opens (openpyxl)"       "python3 -c \"import openpyxl;w=openpyxl.load_workbook('$W/out.xlsx');assert w.active['C3'].value==2250.75\""
check "png is the page"             "python3 -c \"from PIL import Image;im=Image.open('$W/out-1.png');assert im.size[0]>700\""
if [ -n "$jpeg" ]; then
  run --export jpg "$W/rich.pdf" "$W/out.jpg" 100
  check "export jpg"                "python3 -c \"from PIL import Image;assert Image.open('$W/out-1.jpg').format=='JPEG'\""
  echo "compress"
  run --compress "$W/img.pdf" "$W/img_c.pdf" 150
  check "smaller file"              "[ \$(wc -c < '$W/img_c.pdf') -lt \$(wc -c < '$W/img.pdf') ]"
  check "text kept"                 "text '$W/img_c.pdf' | grep -q 'Picture test'"
fi

echo "password protection"
run --protect "$W/rich.pdf" "$W/prot.pdf" open123 owner456
check "needs the password"          "! python3 -c \"import pikepdf;pikepdf.open('$W/prot.pdf')\" 2>/dev/null"
check "AES-256 with either password" "python3 -c \"
import pikepdf
for pw in ('open123','owner456'):
    p=pikepdf.open('$W/prot.pdf',password=pw);assert p.encryption.R==6 and len(p.pages)==1\""
bad=0; for i in $(seq 1 10); do run --protect "$W/rich.pdf" "$W/p$i.pdf" "u$i" "o$i"
  python3 -c "import pikepdf;pikepdf.open('$W/p$i.pdf',password='u$i');pikepdf.open('$W/p$i.pdf',password='o$i')" 2>/dev/null || bad=$((bad+1)); done
check "10 protected files all open with both passwords" "[ $bad -eq 0 ]"

echo "redaction"
run --redact "$W/rich.pdf" "$W/red.pdf" "Quarterly"; rc=$?
check "redaction verified (exit code)" "[ $rc -eq 0 ]"
check "the text is gone"            "! text '$W/red.pdf' | grep -q 'Quarterly'"

echo "forms"
python3 - "$W" <<'PY'
import sys
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
c=canvas.Canvas(sys.argv[1]+'/flat.pdf',pagesize=A4); c.setFont('Helvetica',12)
c.drawString(72,730,'Name: ____________________________'); c.drawString(72,690,'Email:'); c.line(120,688,380,688)
c.rect(72,640,10,10); c.drawString(90,641,'I agree'); c.save()
PY
run --formdetect "$W/flat.pdf" "$W/flat_f.pdf"
check "fields created"              "python3 -c \"import pikepdf;p=pikepdf.open('$W/flat_f.pdf');assert len(p.Root.AcroForm.Fields)==3\""
printf 'Name,Email,I agree\nAnna,a@x.org,yes\n' > "$W/people.csv"
run --formfill "$W/flat_f.pdf" "$W/people.csv" "$W/filled"
check "values filled in"            "python3 -c \"import pikepdf;p=pikepdf.open('$W/filled/row1.pdf');v={str(f.T):str(f.get('/V')) for f in p.Root.AcroForm.Fields};assert v['Name']=='Anna' and v['I agree']=='/Yes',v\""
run --formexport "$W/filled/row1.pdf" "$W/ans.csv"
check "answers exported"            "grep -q 'Anna' '$W/ans.csv'"
run --formtest "$W/rich.pdf" "$W/ft"
check "all kinds filled"            "grep -q 'size=Medium\[x\].*colour=Green when=25.10.2026 who=Ann agree=Yes\[x\]' '$W/ft.txt'"
check "reset clears them"           "grep -q '^reset: size=Off size=Off size=Off colour= when= who= agree=Off' '$W/ft.txt'"
check "flatten leaves no fields"    "grep -q 'fields after flatten: 0' '$W/ft.txt' && text '$W/ft-flat.pdf' | grep -q 'Medium'"

echo "visual compare"
run --visualdiff "$W/rich.pdf" "$W/red.pdf" "$W/vd.png" 1
check "overlay written"             "[ -s '$W/vd.png' ] && grep -q 'ok=1' '$W/vd.png.txt'"

echo "text and Markdown to PDF"
printf '# Shopping list\n\n- **Milk** and bread\n- Eggs\n\n| Item | Qty |\n|---|---|\n| Tea | 2 |\n' > "$W/list.md"
run --topdf "$W/list.md" "$W/list.pdf"
check "heading and table text"      "text '$W/list.pdf' | grep -q 'Shopping list' && text '$W/list.pdf' | grep -q 'Tea'"

echo "hidden data"
python3 -c "
import pikepdf
p=pikepdf.open('$W/rich.pdf'); p.docinfo['/Author']='Secret Person'
p.attachments['payroll.txt']=pikepdf.AttachedFileSpec(p,b'salaries')
p.save('$W/dirty.pdf')"
run --sanitize "$W/dirty.pdf" "$W/clean.pdf"
check "author and attachment gone"  "! grep -aq -e 'Secret Person' -e salaries '$W/clean.pdf' && python3 -c \"import pikepdf;assert len(pikepdf.open('$W/clean.pdf').attachments)==0\""
check "report written"              "[ -s '$W/clean.pdf.txt' ]"
check "text kept"                   "text '$W/clean.pdf' | grep -q 'Quarterly Report'"

echo "page tools"
python3 -c "
import pikepdf
p=pikepdf.open('$W/rich.pdf'); p.pages.append(p.pages[0]); p.save('$W/twice.pdf')"
run --pages "$W/twice.pdf" "$W/once.pdf" nodupes
check "duplicate page removed"      "[ \$(pages '$W/once.pdf') -eq 1 ]"
run --pages "$W/twice.pdf" "$W/hf.pdf" header
check "header and page numbers"     "text '$W/hf.pdf' | grep -q 'Page 2 of 2'"
run --pages "$W/twice.pdf" "$W/rev.pdf" reverse
check "reversed"                    "[ \$(pages '$W/rev.pdf') -eq 2 ]"

echo "picture tools"
python3 -c "
from PIL import Image, ImageDraw
im=Image.new('RGB',(300,300),'white'); d=ImageDraw.Draw(im); d.rectangle((20,20,280,280),fill='green'); im.save('$W/sq.png')"
run --pictest "$W/img.pdf" "$W/pt" "$W/sq.png"
check "all picture tools ran"       "[ \$(grep -c ' ok ' '$W/pt.txt') -eq 11 ]"
check "extracted picture"           "python3 -c \"from PIL import Image;assert Image.open('$W/pt-extract.png').size[0]>1000\""

if [ -n "$jpeg" ]; then
echo "PDF/A-2b copy"
python3 -c "
import pikepdf
p=pikepdf.open('$W/rich.pdf'); p.pages.append(p.pages[0])
link=p.make_indirect(pikepdf.Dictionary(Type=pikepdf.Name.Annot,Subtype=pikepdf.Name.Link,Rect=[72,700,300,720],Dest=[p.pages[1].obj,pikepdf.Name.Fit]))
p.pages[0].obj['/Annots']=pikepdf.Array([link])
p.attachments['a.txt']=pikepdf.AttachedFileSpec(p,b'x')
p.Root.OpenAction=pikepdf.Dictionary(S=pikepdf.Name.JavaScript,JS='app.alert(1)')
p.save('$W/arch.pdf')"
run --pdfa "$W/arch.pdf" "$W/arch-a.pdf"
check "converted"                   "grep -q '^ok=1' '$W/arch-a.pdf.txt'"
check "names PDF/A-2b, sRGB intent" "python3 -c \"
import pikepdf;p=pikepdf.open('$W/arch-a.pdf');m=p.Root.Metadata.read_bytes()
assert b'<pdfaid:part>2</pdfaid:part>' in m and b'<pdfaid:conformance>B</pdfaid:conformance>' in m
oi=p.Root.OutputIntents[0];assert oi.S=='/GTS_PDFA1' and oi.DestOutputProfile.N==3 and '/ID' in p.trailer\""
check "no script, attachment"       "python3 -c \"
import pikepdf;p=pikepdf.open('$W/arch-a.pdf');assert '/OpenAction' not in p.Root and len(p.attachments)==0\""
check "text still searchable"       "text '$W/arch-a.pdf' | grep -q 'Quarterly Report'"
fi

echo "search all PDFs in a folder"
mkdir -p "$W/lib/sub"; cp "$W/rich.pdf" "$W/lib/"; cp "$W/img.pdf" "$W/lib/sub/"
HOME="$W/home" run --search "$W/lib" "Quarterly" "$W/srch.txt"
check "finds the phrase"            "grep -q 'rich.pdf|1|' '$W/srch.txt'"
HOME="$W/home" run --search "$W/lib" "Quarterly" "$W/srch2.txt"
check "second search uses the saved index" "grep -q 'scanned=0' '$W/srch2.txt'"
HOME="$W/home" run --search "$W/lib" "Picture test" "$W/srch3.txt"
check "looks in subfolders"         "grep -q 'sub/img.pdf|1|' '$W/srch3.txt'"

echo "text recognition (OCR)"
"$prog" --ocr "$W/scan.pdf" "$W/scan_ocr.pdf" eng </dev/null >/dev/null 2>"$W/ocr.err"; rc=$?
if [ $rc -eq 5 ]; then echo "  skip OCR engine not in this build"; else
[ $rc -eq 0 ] || echo "  (exit code $rc: $(cat "$W/ocr.err"))"
check "words recognised"            "text '$W/scan_ocr.pdf' | grep -qi 'supplier shall deliver'"
fi

echo "tables inside a form object (PDFs saved from Word on a Mac)"
python3 - "$W" <<'PY'
import sys
from reportlab.pdfgen import canvas
c=canvas.Canvas(sys.argv[1]+'/formtbl.pdf',pagesize=(595,842))
c.beginForm('Page')
rows=[("Course Code","1BMEL306","Semester","III"),("Credits","2","CIE Marks","50")]
xs=[60,230,380,500,560]; top=700; h=24
for i,r in enumerate(rows):
    for j,v in enumerate(r): c.setFont('Times-Roman',11); c.drawString(xs[j]+4, top-i*h-17, v)
for i in range(len(rows)+1): c.line(xs[0],top-i*h,xs[-1],top-i*h)
for x in xs: c.line(x,top,x,top-len(rows)*h)
c.endForm(); c.doForm('Page'); c.save()
PY
run --tableedit "$W/formtbl.pdf" "$W/formtbl_e.pdf" 1 300 690 cell 0 1 "1BMEL307"
check "the cell is changed"          "text '$W/formtbl_e.pdf' | grep -q '1BMEL307'"
check "the old text is gone, not drawn over" "! text '$W/formtbl_e.pdf' | grep -q '1BMEL306' && [ \$(text '$W/formtbl_e.pdf' | grep -o 'Course Code' | wc -l) -eq 1 ]"

echo "fonts and comment shapes"
run --fonts "$W/rich.pdf" "$W/fonts.txt"
check "lists the fonts, not embedded" "grep -q \$'^Helvetica\tType 1\tno' '$W/fonts.txt'"
run --annottest "$W/rich.pdf" "$W/shapes.pdf"
check "polygon, cloud and callout"  "python3 -c \"import pikepdf;p=pikepdf.open('$W/shapes.pdf');assert len(p.pages[0].Annots)==3\""

rm -rf "$work"
[ $fail -eq 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
