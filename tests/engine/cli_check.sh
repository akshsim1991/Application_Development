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

echo "search options"
f() { run --find "$W/search.pdf" "$W/find.txt" "$@"; tr '\n' ' ' < "$W/find.txt"; }
check "plain: inside words too"      "[ \"\$(f cat)\" = 'count=6 1|cat|page 1|cat|page 1|cat|page 1|Cat|page 2|cat|page 2|cat|page ' ]"
check "whole word"                   "[ \"\$(f cat word)\" = 'count=2 1|cat|page 2|cat|page ' ]"
check "match case"                   "f cat case | grep -q '^count=5 '"
check "whole word, in comments too"  "[ \"\$(f cat word comments)\" = 'count=3 1|cat|page 1|cat|comment 2|cat|page ' ]"
check "? is one letter"              "[ \"\$(f 'colo?r' wild)\" = 'count=3 1|Colour|page 1|colour|page 1|colour|page ' ]"
check "* any letters, whole words"   "[ \"\$(f 'colo*r' wild word)\" = 'count=4 1|Colour|page 1|color|page 1|colour|page 1|color|page ' ]"
check "* at the end: the whole word" "f 'cat*' wild word comments | grep -q 'count=7 .*1|category|page 1|Cats|page .*2|catalogue|comment'"
check "\\* is a real star"           "[ \"\$(f '5\\*3' wild)\" = 'count=1 1|5*3|page ' ]"
check "a star alone finds nothing"   "f '*' wild | grep -q '^count=0 '"

echo "split, merge, two-sided scans"
ol() { python3 "$root/tests/outline_dump.py" "$1" 2>/dev/null; }
run --split "$W/book.pdf" "$W/sp" every 4
check "every 4 pages: 3 files"       "[ \"\$(cat '$W/sp.txt' | tr '\n' ' ')\" = '1|1-4| 2|5-8| 3|9-9| ' ] && [ \$(pages '$W/sp-3.pdf') -eq 1 ]"
run --split "$W/book.pdf" "$W/sb" bookmarks 1
check "at the chapters, named"       "[ \"\$(cat '$W/sb.txt' | tr '\n' ' ')\" = '1|1-3|Chapter One 2|4-6|Chapter Two 3|7-9|Chapter Three ' ]"
check "each part keeps its bookmarks" "[ \"\$(ol '$W/sb-2.pdf')\" = '3 Chapter Two->1;   Section 5.1->2' ]"
check "the part's own pages"         "text '$W/sb-2.pdf' | grep -q 'Book page 4' && ! text '$W/sb-2.pdf' | grep -q 'Book page 3'"
run --split "$W/heavy.pdf" "$W/ss" size 2
check "no part over 2 MB, none empty" "[ -f '$W/ss-2.pdf' ] && for f in '$W'/ss-*.pdf; do [ \$(wc -c < \"\$f\") -le 2097152 ] || exit 1; done"
check "all pages in the parts"       "[ \$(( \$(for f in '$W'/ss-*.pdf; do pages \"\$f\"; done | paste -sd+ -) )) -eq 6 ]"
run --merge "$W/mk.pdf" keep "$W/book.pdf" "$W/appendix.pdf"
check "merge keeps the bookmarks"    "[ \"\$(ol '$W/mk.pdf')\" = '11 Chapter One->1;   Section 2.1->2; Chapter Two->4;   Section 5.1->5; Chapter Three->7; Appendix A->10' ]"
run --merge "$W/mf.pdf" files "$W/book.pdf" "$W/appendix.pdf"
check "merge: a bookmark per file"   "ol '$W/mf.pdf' | grep -q '^11 book->1;   Chapter One->1; .*appendix->10;   Appendix A->10\$'"
run --merge "$W/mn.pdf" none "$W/book.pdf" "$W/appendix.pdf"
check "merge without bookmarks"      "[ \"\$(ol '$W/mn.pdf')\" = '11 ' ]"
run --interleave "$W/duplex.pdf" "$W/il1.pdf" reversed
check "one file: fronts, then backs reversed" "[ \"\$(text '$W/il1.pdf' | tr -d '\\f' | grep -E 'Front|Back' | paste -sd, -)\" = 'Front 1,Back 1,Front 2,Back 2,Front 3,Back 3' ]"
run --interleave "$W/fronts.pdf" "$W/il2.pdf" reversed "$W/backs.pdf"
check "two files: fronts and backs"  "[ \"\$(text '$W/il2.pdf' | tr -d '\\f' | grep -E 'Front|Back' | paste -sd, -)\" = 'Front 1,Back 1,Front 2,Back 2,Front 3,Back 3' ]"

echo "text comparison, word by word"
run --textdiff "$W/version1.pdf" "$W/version2.pdf" "$W/td.txt" "$W/td-marked.pdf" "$W/td-report.pdf"
check "three changes, the moved line not one" "head -1 '$W/td.txt' | grep -q '^changes=3 replaced=1 inserted=1 removed=1 same=34 rough=0$'"
check "replaced: brown -> red"       "grep -A3 'Replaced\*\*' '$W/td.txt' | grep -q 'Was: .*brown' && grep -A3 'Replaced\*\*' '$W/td.txt' | grep -q 'Now: .*red'"
check "inserted after page 2's last words" "grep -q 'Inserted\*\* .* page 2 of the first, page 2 of the second' '$W/td.txt'"
check "removed on page 3"            "grep -A2 'Removed\*\*' '$W/td.txt' | grep -q 'Was: .*Signed by the manager'"
check "marked copy: highlight, note, strike-out" "python3 -c \"
import pikepdf;p=pikepdf.open('$W/td-marked.pdf')
got=[(i+1,str(a.Subtype),str(a.get('/Contents',''))) for i,pg in enumerate(p.pages) for a in pg.get('/Annots',[])]
assert got==[(1,'/Highlight','Now: red'),(2,'/Text','Inserted: Late payments incur a fee of two percent.'),(3,'/StrikeOut','Removed')],got\""
check "the report as a PDF"          "text '$W/td-report.pdf' | grep -q 'Changes: 3 (1 replaced, 1 inserted, 1 removed)'"
run --textdiff "$W/version1.pdf" "$W/version1.pdf" "$W/td0.txt"
check "the same document: no changes" "head -1 '$W/td0.txt' | grep -q '^changes=0 .* same=39 '"

echo "attachments and layers"
run --attach "$W/layers.pdf" list "$W/at.txt"
check "lists the attachment"         "[ \"\$(cat '$W/at.txt')\" = 'notes.txt|18' ]"
run --attach "$W/layers.pdf" get 0 "$W/at-got.txt"
check "saves it, byte for byte"      "[ \"\$(cat '$W/at-got.txt')\" = 'Attached notes: 42' ]"
run --attach "$W/layers.pdf" add "$W/extra.csv" "$W/at-add.pdf"
check "adds a file (pikepdf reads it)" "python3 -c \"import pikepdf;p=pikepdf.open('$W/at-add.pdf');a=p.attachments;d=a['extra.csv'].get_file().read_bytes();assert sorted(a)==['extra.csv','notes.txt'] and d.startswith(b'a,b') and len(d)==8\""
run --attach "$W/at-add.pdf" remove 0 "$W/at-rem.pdf"
check "removes one"                  "python3 -c \"import pikepdf;p=pikepdf.open('$W/at-rem.pdf');assert list(p.attachments)==['notes.txt']\""
run --layers "$W/layers.pdf" "$W/ly.txt"
check "lists the layers, shown or hidden" "[ \"\$(paste -sd, - < '$W/ly.txt')\" = 'Plan A|on,Plan B|off' ]"
run --layers "$W/layers.pdf" "$W/ly2.txt" "$W/ly-flip.pdf" "Plan A=off" "Plan B=on"
check "hides one, shows the other"   "[ \"\$(paste -sd, - < '$W/ly2.txt')\" = 'Plan A|off,Plan B|on' ]"
run --export png "$W/layers.pdf" "$W/ly1.png" 36; run --export png "$W/ly-flip.pdf" "$W/ly2.png" 36
check "drawn as set (before and after)" "python3 -c \"
from PIL import Image
def at(f,x,y):
    im=Image.open(f).convert('RGB');w,h=im.size;return im.getpixel((int(x/595*w),int((842-y)/842*h)))
assert at('$W/ly1-1.png',400,700)==(0,0,255) and at('$W/ly1-1.png',150,400)==(255,255,255)
assert at('$W/ly2-1.png',400,700)==(255,255,255) and at('$W/ly2-1.png',150,400)==(255,0,0)\""

echo "repair and Fast Web View"
for f in shifted noindex; do
  run --repair "$W/damaged-$f.pdf" "$W/rep-$f.pdf"; rc=$?
  check "repairs: $f (9 pages, readable)" "[ $rc -eq 0 ] && grep -q '^opened=1 fixed=1 pages=9 bad=0' '$W/rep-$f.pdf.txt' && [ \$(text '$W/rep-$f.pdf' | grep -c 'Book page') -eq 9 ]"
  if command -v pdftotext >/dev/null; then check "repairs: $f, clean to poppler" "! pdftotext '$W/rep-$f.pdf' - 2>&1 | grep -qi error"; fi
done
check "says what was wrong"          "grep -q 'cut short' '$W/rep-noindex.pdf.txt' && grep -q 'index of the file' '$W/rep-shifted.pdf.txt'"
check "keeps the bookmarks"          "python3 '$root/tests/outline_dump.py' '$W/rep-noindex.pdf' | grep -q '^9 Chapter One->1'"
run --repair "$W/book.pdf" "$W/rep-ok.pdf"
check "an intact file: nothing wrong" "grep -q '^opened=1 fixed=0 pages=9' '$W/rep-ok.pdf.txt'"
run --merge "$W/inc.pdf" keep "$W/book.pdf" "$W/appendix.pdf"
for f in book book-outlines rich search heavy inc; do
  run --linearize "$W/$f.pdf" "$W/web-$f.pdf"
  check "fast web view: $f (qpdf's check, looks the same)" "python3 -c \"
import sys,pikepdf,subprocess,hashlib
p=pikepdf.open('$W/web-$f.pdf');assert p.is_linearized and p.check_linearization() and not p.get_warnings(),p.get_warnings()
import shutil
img=lambda f: hashlib.md5(subprocess.run(['pdftoppm','-r','30','-gray',f],capture_output=True).stdout).hexdigest() if shutil.which('pdftoppm') else ''
assert len(p.pages)==len(pikepdf.open('$W/$f.pdf').pages) and img('$W/$f.pdf')==img('$W/web-$f.pdf')\""
done
check "bookmarks kept"               "[ \"\$(python3 '$root/tests/outline_dump.py' '$W/web-inc.pdf')\" = \"\$(python3 '$root/tests/outline_dump.py' '$W/inc.pdf')\" ]"

echo "undo history"
run --history "$W/book.pdf" "$W/hist.txt"
check "each step named"              "grep -q '^steps: Rotate pages; Insert a blank page; Reverse the page order; pages=10$' '$W/hist.txt'"
check "back to the start at once"    "grep -q '^back=1 pages=9 undo=0 redo=3 next=Rotate pages$' '$W/hist.txt'"
check "forward one step"             "grep -q '^forward=1 pages=9 undo=1 redo=2$' '$W/hist.txt'"

echo "accessibility check and fixes"
run --access "$W/access.pdf" "$W/acc.txt" "$W/acc-fixed.pdf"
before() { sed -n '1,/^--- fixed:/p' "$W/acc.txt"; }
after() { sed -n '/^--- after:/,$p' "$W/acc.txt"; }
check "finds the problems"           "before | grep -q '^title|failed|fixable|Only a placeholder' && before | grep -q '^lang|failed|fixable' && before | grep -q '^showtitle|failed|fixable' && before | grep -q '^links|failed|fixable' && before | grep -q '^fields|failed|fixable' && before | grep -q '^tabs|failed|fixable' && before | grep -q '^tagged|failed|' && before | grep -q '^text|failed|1 page is only a picture (page 2)'"
check "fixes what can be fixed"      "after | grep -q '^title|passed|.*Annual Safety Report' && after | grep -q '^lang|passed|Language: en-GB' && after | grep -q '^showtitle|passed' && after | grep -q '^links|passed' && after | grep -q '^fields|passed' && after | grep -q '^tabs|passed'"
check "and leaves the rest to a person" "after | grep -q '^tagged|failed|' && after | grep -q '^contrast|person|'"
check "written into the file (pikepdf)" "python3 -c \"
import pikepdf;p=pikepdf.open('$W/acc-fixed.pdf')
assert str(p.docinfo.Title)=='Annual Safety Report' and str(p.Root.Lang)=='en-GB' and p.Root.ViewerPreferences.DisplayDocTitle
assert [str(a.Contents) for pg in p.pages for a in pg.get('/Annots',[]) if a.Subtype=='/Link']==['Link to https://example.org/safety']
assert [str(f.TU) for f in p.Root.AcroForm.Fields]==['full name'] and p.pages[0].Tabs=='/S'\""
run --access "$W/tagged.pdf" "$W/acc2.txt"
check "a tagged document: tags pass, a picture without a description fails" "grep -q '^tagged|passed' '$W/acc2.txt' && grep -q '^title|passed|.*Search Samples' '$W/acc2.txt' && grep -q '^alt|failed|1 picture has no alternate text' '$W/acc2.txt'"

echo "comment threads and review"
run --review "$W/search.pdf" "$W/rev.txt" "$W/rev.pdf"
check "two comments found, all changes made" "grep -q '^threads=2$' '$W/rev.txt' && grep -q '^done=1111 ' '$W/rev.txt'"
check "the summary: replies in order, statuses" "grep -q 'Accepted:\*\* 1 .*Rejected:\*\* 1 .*Open:\*\* 0' '$W/rev.txt' && grep -A3 'Accepted (Asha)' '$W/rev.txt' | grep -q 'cat photo' && grep -q 'Asha.*: Agreed, please change it.' '$W/rev.txt' && grep -q 'Ravi.*: Done in version 2.' '$W/rev.txt' && grep -q 'Rejected (Ravi)' '$W/rev.txt'"
check "stored as the PDF standard has it (pikepdf)" "python3 -c \"
import pikepdf;p=pikepdf.open('$W/rev.pdf')
a=[x for pg in p.pages for x in pg.get('/Annots',[])]
top=[x for x in a if '/IRT' not in x]
rep=[x for x in a if '/IRT' in x and '/State' not in x]
st=[x for x in a if '/State' in x]
assert len(top)==2 and len(rep)==2 and len(st)==2
assert all(r.IRT.objgen==top[0].objgen and r.RT=='/R' for r in rep)
assert sorted(str(x.State) for x in st)==['Accepted','Rejected'] and all(str(x.StateModel)=='Review' for x in st)\""
run --review "$W/rev.pdf" "$W/rev2.txt"
check "read back from the saved file" "grep -q 'Comments:\*\* 2 (2 replies)' '$W/rev2.txt'"

echo "read aloud: words on the page, sentences"
run --readaloud "$W/version1.pdf" "$W/ra.txt"
check "each word found on its page"  "grep -q '^Payment=1:Payment$' '$W/ra.txt' && grep -q '^Second=2:Second$' '$W/ra.txt' && grep -q '^Final=3:Final$' '$W/ra.txt'"
check "back, here, next sentence"     "grep -q '^back=The quick|here=Payment i|next=Second pa$' '$W/ra.txt'"

echo "form calculations and checks"
run --formcalc "$W/order.pdf" "$W/fc.txt" "$W/fc.pdf"
check "set and read back"            "grep -q '^set=1111111$' '$W/fc.txt' && grep -q '^calc total=Sum price tax$' '$W/fc.txt' && grep -q '^rule qty kind=2 dec=0 min=1 max=100$' '$W/fc.txt'"
check "the total worked out"         "grep -q '^total=118.59$' '$W/fc.txt'"
check "bad values found, good ones pass" "grep -q '^problem qty: Type a number from 1 to 100.$' '$W/fc.txt' && grep -q '^problem email:' '$W/fc.txt' && grep -q '^problem date: That day does not exist' '$W/fc.txt' && ! grep -q '^problem phone' '$W/fc.txt' && grep -q '^ok: |$' '$W/fc.txt'"
check "Acrobat's own scripts and order (pikepdf)" "python3 -c \"
import pikepdf;p=pikepdf.open('$W/fc.pdf');f={str(x.T):x for x in p.Root.AcroForm.Fields}
assert str(f['total'].AA.C.JS)=='AFSimple_Calculate(\\\"SUM\\\", new Array(\\\"price\\\", \\\"tax\\\"));'
assert str(f['qty'].AA.V.JS)=='AFRange_Validate(true, 1, true, 100);' and 'AFNumber_Keystroke(0' in str(f['qty'].AA.K.JS)
assert [str(x.T) for x in p.Root.AcroForm.CO]==['total']\""

echo "pen pressure"
run --pressureink "$W/order.pdf" "$W/pen.pdf"; run --export png "$W/pen.pdf" "$W/pen.png" 72
check "thin where light, thick where hard" "python3 -c \"
from PIL import Image
im=Image.open('$W/pen-1.png').convert('L');w,h=im.size
t=lambda x: sum(1 for yy in range(int(412/842*h),int(472/842*h)) if im.getpixel((int(x/595*w),yy))<128)
a,b,c=t(110),t(300),t(490);assert 2<=a<b<c and c>=12,(a,b,c)\""
check "the stroke kept for other readers (pikepdf)" "python3 -c \"
import pikepdf;p=pikepdf.open('$W/pen.pdf');a=[x for x in p.pages[0].Annots if x.Subtype=='/Ink'][0];assert len(a.InkList[0])==82 and '/AP' in a\""

echo "sensitive information"
run --sensitive "$W/sensitive.pdf" "$W/sens.txt"; rc=$?
check "lists without changing (exit code)" "[ $rc -eq 0 ] && grep -q '^found=10$' '$W/sens.txt'"
for want in "E-mail address|priya.sharma@example.co.in" "E-mail address|p_s+bills@mail.example.com" "Phone number|+91 98450 12345" \
            "Phone number|(080) 2345 6789" "Phone number|9876543210" "PAN|ABCPE1234F" "Bank account or IFSC|123456789012345" \
            "Bank account or IFSC|SBIN0001234"; do
  check "finds $want" "grep -qF '|$want' '$W/sens.txt'"; done
aad="$(cat "$W/sensitive-aadhaar.txt")"
check "finds the Aadhaar number (check digit valid)" "grep -qF 'Aadhaar number|${aad:0:4} ${aad:4:4} ${aad:8:4}' '$W/sens.txt'"
check "second page searched too"    "grep -qF '2|E-mail address|accounts@example.org' '$W/sens.txt'"
check "look-alikes left alone (dates, amounts, ISBN, wrong check digit, bad PAN)" "! grep -qE '2026-11|1,234|978-3|ABCDE1234F|localhost|[|]4521|[|]12345\$' '$W/sens.txt' && [ \$(grep -c 'Aadhaar' '$W/sens.txt') -eq 1 ]"
run --sensitive "$W/sensitive.pdf" "$W/sens2.txt" "$W/sens-red.pdf"; rc=$?
check "all redacted, none left in the file" "[ $rc -eq 0 ] && grep -q 'left=0' '$W/sens2.txt'"
check "redacted text gone (pdfminer)" "! text '$W/sens-red.pdf' | grep -qE 'priya|ABCPE|98450'"
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
check "words recognised"            "text '$W/scan_ocr.pdf' | tr -s ' \\n' ' ' | grep -qi 'supplier shall deliver'"; echo "  (the text found: $(text "$W/scan_ocr.pdf" | tr -s ' \n' ' ' | head -c 300))"
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

echo "digital signatures"
if python3 - "$W" <<'PY' 2>/dev/null
import sys, datetime
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.serialization import pkcs12
key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Check Signer")])
now = datetime.datetime.now(datetime.timezone.utc)
cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1)).not_valid_after(now + datetime.timedelta(days=30)).sign(key, hashes.SHA256()))
enc = (serialization.PrivateFormat.PKCS12.encryption_builder().kdf_rounds(2048)
       .key_cert_algorithm(pkcs12.PBES.PBESv1SHA1And3KeyTripleDESCBC).hmac_hash(hashes.SHA1()).build(b"pw"))
open(sys.argv[1] + "/t.p12", "wb").write(pkcs12.serialize_key_and_certificates(b"Check Signer", key, cert, None, enc))
PY
then
  "$prog" --sign "$W/rich.pdf" "$W/signed.pdf" "$W/t.p12" pw - visible </dev/null >/dev/null 2>"$W/sign.err"; rc=$?
  if [ $rc -eq 5 ] && grep -q "not available" "$W/sign.err"; then echo "  skip signing not in this build"; else
  [ $rc -eq 0 ] || echo "  (exit code $rc: $(cat "$W/sign.err" "$W/signed.pdf.error.txt" 2>/dev/null))"
  check "signed copy written"         "[ -s '$W/signed.pdf' ]"
  run --checksig "$W/signed.pdf" "$W/sig.txt"
  check "own check: valid (signer not trusted here)" "grep -q '^VALID' '$W/sig.txt' && grep -q 'Check Signer' '$W/sig.txt'"
  python3 -c "import sys; w=sys.argv[1]; d=bytearray(open(w+'/signed.pdf','rb').read()); d[60]^=1; open(w+'/tampered.pdf','wb').write(d)" "$W"
  run --checksig "$W/tampered.pdf" "$W/sig2.txt"
  check "own check: tampering found"  "grep -q '^CHANGED' '$W/sig2.txt'"
  check "still opens (pikepdf), one signature field" "python3 -c \"import pikepdf;p=pikepdf.open('$W/signed.pdf');assert len(p.Root.AcroForm.Fields)==1\""
  fi
else echo "  skip no cryptography module"; fi

echo "batch: a whole folder"
mkdir -p "$W/bin/sub" "$W/bout"; cp "$W/rich.pdf" "$W/bin/"; cp "$W/rich.pdf" "$W/bin/sub/other.pdf"
printf '[batch]\ninput=%s\noutput=%s\nsubfolders=1\nwatermark=DRAFT\nuserPassword=open1\n' "$W/bin" "$W/bout" > "$W/preset.ini"
run --batch "$W/preset.ini"; rc=$?
check "both files done (subfolder kept)" "[ $rc -eq 0 ] && [ -s '$W/bout/rich.pdf' ] && [ -s '$W/bout/sub/other.pdf' ]"
check "watermark and password set"   "python3 -c \"import pikepdf;p=pikepdf.open('$W/bout/sub/other.pdf',password='open1');assert p.is_encrypted\" && python3 -c \"import sys;from pdfminer.high_level import extract_text;t=extract_text(sys.argv[1],password='open1');assert 'DRAFT' in t or 'AFT' in t\" '$W/bout/rich.pdf'"

echo "one font out of a collection file (edited text in fonts such as Helvetica on the Mac)"
ttc=""; for f in /System/Library/Fonts/Helvetica.ttc /usr/share/fonts/truetype/wqy/wqy-zenhei.ttc; do [ -f "$f" ] && { ttc="$f"; break; }; done
if [ -n "$ttc" ] && python3 -c "import fontTools" 2>/dev/null; then
  run --sfnttest "$ttc" 1 "$W/one.ttf"
  check "a font file of its own, checksums right (fontTools)" "python3 -c \"
from fontTools.ttLib import TTFont, TTCollection
f=TTFont('$W/one.ttf', checkChecksums=2); c=TTCollection('$ttc').fonts[1]
assert f['maxp'].numGlyphs==c['maxp'].numGlyphs and f['name'].getDebugName(4)==c['name'].getDebugName(4)\""
else echo "  skip no collection font or fontTools here"; fi

echo "numbered list items (editing a paragraph)"
python3 - "$W" <<'PY'
import sys
from reportlab.pdfgen import canvas
from reportlab.pdfbase.pdfmetrics import stringWidth
c=canvas.Canvas(sys.argv[1]+'/list.pdf',pagesize=(612,792))
items=[("2.",["Explain the types of controllers with their transfer functions and mention","their advantages."]),
       ("3.",["Explain proportional-plus-integral (PI) controller with its characteristics","and block diagram."]),
       ("4.",["Explain the proportional-derivative (PD) controller with its","characteristics and block diagram."])]
y=700; right=520
for num,lines in items:
    c.setFont('Times-Roman',14); c.drawString(72,y,num)
    for k,l in enumerate(lines):
        t=c.beginText(); t.setFont('Times-Roman',14); t.setTextOrigin(97,y)
        if k==0:   # justified: the spaces widened to reach the right edge
            gaps=l.count(' '); t.setWordSpace((right-97-stringWidth(l,'Times-Roman',14))/gaps)
        else: t.setWordSpace(0)
        t.textOut(l); c.drawText(t); y-=18.5
c.save()
PY
run --paratest "$W/list.pdf" "$W/list_e.pdf" "$W/list.txt" 200 667 append "Draw it neatly."
check "item 3 alone, both its lines"   "grep -q '^text=Explain proportional-plus-integral (PI) controller with its characteristics and block diagram.$' '$W/list.txt'"
check "justified, as it is set"        "grep -q 'align=3' '$W/list.txt'"
check "numbers and the other items kept" "text '$W/list_e.pdf' > '$W/list_e.txt'; grep -qx '3.' '$W/list_e.txt' && grep -q 'Explain proportional-plus-integral' '$W/list_e.txt' && grep -q 'their advantages.' '$W/list_e.txt' && grep -q 'Draw it neatly.' '$W/list_e.txt'"

echo "fonts and comment shapes"
run --fonts "$W/rich.pdf" "$W/fonts.txt"
check "lists the fonts, not embedded" "grep -q \$'^Helvetica\tType 1\tno' '$W/fonts.txt'"
run --annottest "$W/rich.pdf" "$W/shapes.pdf"
check "polygon, cloud and callout"  "python3 -c \"import pikepdf;p=pikepdf.open('$W/shapes.pdf');assert len(p.pages[0].Annots)==3\""

echo "page colours: blackboard (AMOLED black)"
python3 - "$W" <<'PY'
import sys
from reportlab.pdfgen import canvas
from reportlab.lib.utils import ImageReader
from PIL import Image
d=sys.argv[1]
im=Image.new("RGB",(100,100),(30,120,200)); im.save(d+"/bb_pic.png")
c=canvas.Canvas(d+"/bb.pdf",pagesize=(400,300))
c.setFillColorRGB(0,0,0); c.rect(20,240,80,30,fill=1,stroke=0)          # black "text" block
c.setFillColorRGB(0.85,0.1,0.1); c.rect(120,240,80,30,fill=1,stroke=0)  # red text
c.setFillColorRGB(1,1,0.5); c.rect(220,240,80,30,fill=1,stroke=0)       # a light yellow highlight
c.drawImage(ImageReader(d+"/bb_pic.png"),50,50,width=100,height=100)
c.save()
PY
run --pagecolours "$W/bb.pdf" "$W/bb_inv.ppm" blackboard
run --pagecolours "$W/bb.pdf" "$W/bb_orig.ppm" blackboard-original
pix() { python3 -c "from PIL import Image;print(*Image.open('$1').getpixel(($2,$3)))"; }   # at 2x: (x*2, (300-y)*2)
check "inverted: paper pure black"        "[ \"\$(pix $W/bb_inv.ppm 700 20)\" = '0 0 0' ]"
check "inverted: black text white"        "[ \"\$(pix $W/bb_inv.ppm 120 90)\" = '255 255 255' ]"
check "inverted: picture inverted"        "[ \"\$(pix $W/bb_inv.ppm 200 400)\" = '225 135 55' ]"
check "original: paper pure black"        "[ \"\$(pix $W/bb_orig.ppm 700 20)\" = '0 0 0' ]"
check "original: black text white"        "[ \"\$(pix $W/bb_orig.ppm 120 90)\" = '255 255 255' ]"
check "original: red stays red"           "python3 -c \"from PIL import Image;r,g,b=Image.open('$W/bb_orig.ppm').getpixel((320,90));assert r>200 and g<60 and b<60\""
check "original: picture as it is"        "[ \"\$(pix $W/bb_orig.ppm 200 400)\" = '30 120 200' ] && grep -q pictures=1 '$W/bb_orig.ppm.txt'"
check "original: highlight a dark tint"   "python3 -c \"from PIL import Image;r,g,b=Image.open('$W/bb_orig.ppm').getpixel((520,90));assert 100<r<160 and 100<g<160 and b<10\""

echo "LaTeX fonts: the style a paragraph is edited in"
python3 - "$W" <<'PY'
import sys, pikepdf
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
d=sys.argv[1]
import os
pairs=[('/usr/share/fonts/truetype/liberation/LiberationSerif-Bold.ttf','/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf'),
       ('/System/Library/Fonts/Supplemental/Times New Roman Bold.ttf','/System/Library/Fonts/Supplemental/Times New Roman.ttf'),
       ('C:/Windows/Fonts/timesbd.ttf','C:/Windows/Fonts/times.ttf')]
pair=next((p for p in pairs if os.path.exists(p[0]) and os.path.exists(p[1])), None)
if not pair: print('  skip no serif TrueType font here'); sys.exit(0)
pdfmetrics.registerFont(TTFont('SB',pair[0]))
pdfmetrics.registerFont(TTFont('SR',pair[1]))
c=canvas.Canvas(d+'/texraw.pdf',pagesize=(595,842))
c.setFont('SR',17); c.drawString(72,780,'2. Enumerate the limitations of both online and offline')
c.setFont('SB',17); c.drawString(72,735,'Module 5 (b)')
c.setFont('SR',17); c.drawString(72,700,'1. Define a material handling system.')
c.save()
p=pikepdf.open(d+'/texraw.pdf')   # the fonts named as pdfTeX names LaTeX's Times (bold "Medi"), with its font flags
for f in p.pages[0].Resources.Font.values():
    new=pikepdf.Name('/ABCDEF+NimbusRomNo9L-Medi' if 'Bold' in str(f.BaseFont) else '/GHIJKL+NimbusRomNo9L-Regu')
    for x in [f]+list(f.get('/DescendantFonts') or []):
        x.BaseFont=new
        if '/FontDescriptor' in x: x.FontDescriptor.FontName=new; x.FontDescriptor.Flags=4
p.save(d+'/tex.pdf')
PY
if [ -f "$W/tex.pdf" ]; then
run --paratest "$W/tex.pdf" "$W/tex_e.pdf" "$W/tex.txt" 120 740 append "Revised"   # (no leading "-": the Mac app takes "-word" for a setting)
check "a LaTeX bold heading is a bold serif" "grep -q 'style family=1 bold=1 italic=0 size=17.0 font=NimbusRomNo9L-Medi' '$W/tex.txt'"
run --paratest "$W/tex.pdf" "$W/tex_b.pdf" "$W/tex2.txt" 120 703 same
check "its body text a regular serif"      "grep -q 'style family=1 bold=0 italic=0' '$W/tex2.txt'"
check "a heading grows along its line, not onto a second" "python3 '$root/tests/engine/sameline.py' '$W/tex_e.pdf' Module Revised"
if ! python3 "$root/tests/engine/sameline.py" "$W/tex_e.pdf" Module Revised >/dev/null 2>&1; then   # what the Mac made of it
  echo "  -- the edit's report:"; sed 's/^/     /' "$W/tex.txt"
  python3 -c "
import pikepdf,sys
p=pikepdf.open('$W/tex_e.pdf'); pg=p.pages[0]
print('     fonts:', {str(k): str(v.get('/BaseFont')) for k,v in pg.Resources.Font.items()})
print('     content:', pg.Contents.read_bytes()[:1500] if not isinstance(pg.obj.Contents, pikepdf.Array) else b''.join(c.read_bytes() for c in pg.obj.Contents)[:1500])
from pdfminer.high_level import extract_pages
from pdfminer.layout import LTChar
def walk(o):
    if isinstance(o, LTChar): print('     char %r %s at %.1f,%.1f' % (o.get_text(), o.fontname, o.x0, o.y0))
    elif hasattr(o,'__iter__'):
        for x in o: walk(x)
for page in extract_pages('$W/tex_e.pdf'): walk(page)
" 2>&1 | head -80
fi
fi

echo "a bold label in a paragraph, a dash, two edits in a row"
python3 - "$W" <<'PY'
import sys
from reportlab.lib.pagesizes import A4
from reportlab.platypus import SimpleDocTemplate, Paragraph
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_JUSTIFY
st=ParagraphStyle('b',fontName='Helvetica',fontSize=9.5,leading=13,alignment=TA_JUSTIFY,spaceBefore=8)
SimpleDocTemplate(sys.argv[1]+'/label.pdf',pagesize=A4).build([
  Paragraph('<b>Implementation principle:</b> Feature backlog, not a mandate to implement everything at once. Prioritize correctness, file integrity, performance, and maintainability.',st),
  Paragraph('<b>License and cost controls:</b> Paid third-party licenses. Some integrations (cloud APIs, patented codecs, proprietary CAD formats, signing/notarization) can carry costs or restrictions. Do not promise these without verification. Free software can still incur developer distribution costs.',st),
  Paragraph('1. Multi-tab interface \u2014 Provide a discoverable, keyboard-accessible workflow with clear errors, consistent state, and appropriate platform-specific handling.',st)])
PY
run --paratest "$W/label.pdf" "$W/label1.pdf" "$W/label1.txt" 100 722 append "Reviewed yearly."
check "the paragraph is the whole paragraph, label and all, in its body style" "grep -q '^lines=3 ' '$W/label1.txt' && grep -q 'style family=0 bold=0' '$W/label1.txt' && grep -q '^text=License and cost controls: *Paid third-party' '$W/label1.txt'"
check "the bold label stays bold, the rest regular" "python3 -c \"
from pdfminer.high_level import extract_pages
from pdfminer.layout import LTChar
ch=[]
def w(o):
    if isinstance(o,LTChar): ch.append((o.get_text(),o.fontname,round(o.y0)))
    elif hasattr(o,'__iter__'):
        [w(x) for x in o]
[w(p) for p in extract_pages('$W/label1.pdf')]
line=[c for c in ch if c[2]==718]
bold=''.join(c[0] for c in line if 'Bold' in c[1]); reg=''.join(c[0] for c in line if 'Bold' not in c[1])
assert bold.replace(' ','')=='Licenseandcostcontrols:', bold
assert reg.replace(' ','').startswith('Paidthird-party'), reg
assert any('Reviewed' in ''.join(c[0] for c in ch if c[2]==y) for y in {c[2] for c in ch})
\""
run --paratest "$W/label1.pdf" "$W/label2.pdf" "$W/label2.txt" 100 675 append "Tested — done."
check "a second edit on the page: the item whole, a dash accepted" "grep -q '^lines=2 .*result=ok' '$W/label2.txt' && text '$W/label2.pdf' | grep -q 'Tested — done.'"

echo "edit mode: the outlined blocks"
run --blocks "$W/rich.pdf" "$W/blocks.txt"
check "every paragraph of the report outlined, as a click edits it" "grep -q 'pictures=0 text=15 same=15' '$W/blocks.txt' && grep -q '^text .* same Quarterly Report' '$W/blocks.txt'"
run --blocks "$W/bb.pdf" "$W/blocks2.txt"
check "the picture outlined"  "grep -q '^picture 50 50 150 150' '$W/blocks2.txt'"
python3 - "$W" <<'PY'
import sys
from reportlab.pdfgen import canvas
c=canvas.Canvas(sys.argv[1]+'/colours.pdf',pagesize=(400,300)); c.setFont('Helvetica',14)
for i,(col,t) in enumerate([((0,0,0),'Black line one'),((0,0,0),'Black line two'),((0.85,0.1,0.1),'Red line'),((0,0,0),'Black again')]):
    c.setFillColorRGB(*col); c.drawString(40,250-i*17,t)
c.save()
PY
run --blocks "$W/colours.pdf" "$W/blocks3.txt"
check "a line in another colour is a paragraph of its own" "grep -q 'text=3 same=3' '$W/blocks3.txt' && grep -q 'same Black line one Black line two' '$W/blocks3.txt'"

echo "convert to PDF (the right-click menu)"
mkdir -p "$W/conv"
python3 -c "from PIL import Image;Image.new('RGB',(1200,900),(200,50,50)).save('$W/conv/Photo.png')"
printf '# Notes\n\nSome text.\n' > "$W/conv/notes.md"; printf 'plain text\n' > "$W/conv/a.txt"; echo junk > "$W/conv/bad.png"; echo x > "$W/conv/doc.docx"
check "a picture, Markdown and text made into PDFs" "'$prog' --convert '$W/conv/Photo.png' '$W/conv/notes.md' '$W/conv/a.txt' </dev/null >/dev/null 2>&1 && [ -f '$W/conv/Photo.pdf' ] && [ -f '$W/conv/notes.pdf' ] && [ -f '$W/conv/a.pdf' ]"
check "the picture's page: no larger than A4's long side" "python3 -c \"import pikepdf;p=pikepdf.open('$W/conv/Photo.pdf');b=[float(x) for x in p.pages[0].MediaBox];assert b[2]==842 and abs(b[3]-631.5)<0.6\""
check "the Markdown's words in its PDF"   "text '$W/conv/notes.pdf' | grep -q 'Some text.'"
python3 -c "from PIL import Image;Image.new('RGB',(400,300),(30,120,200)).save('$W/conv/really-png.jpg',format='PNG')"
check "a PNG named .jpg: its picture, not a blank page" "'$prog' --convert '$W/conv/really-png.jpg' </dev/null >/dev/null 2>&1 && python3 -c \"import pikepdf;p=pikepdf.open('$W/conv/really-png.pdf');i=next(iter(p.pages[0].images.values()));assert '/DCTDecode' not in str(i.get('/Filter'))\""
check "an existing PDF is not replaced"   "'$prog' --convert '$W/conv/Photo.png' </dev/null >/dev/null 2>&1 && [ -f '$W/conv/Photo (2).pdf' ]"
check "a damaged picture, a Word file and a missing file refused" "! '$prog' --convert '$W/conv/bad.png' </dev/null >/dev/null 2>&1 && ! '$prog' --convert '$W/conv/doc.docx' </dev/null >/dev/null 2>&1 && ! '$prog' --convert '$W/conv/none.png' </dev/null >/dev/null 2>&1 && [ ! -f '$W/conv/bad.pdf' ] && [ ! -f '$W/conv/doc.pdf' ]"

echo "a page printed from a browser (gradient background, shadow, symbols in a Type3 font, a footer at the bottom)"
# fixtures/printed-slide.pdf: fixtures/printed-slide.html printed to PDF by Chromium. PDFium cannot write a pattern fill (the
# gradient became black) or Type3 text (the arrows were left out); such objects become pictures of themselves.
S="$here/fixtures/printed-slide.pdf"
run --paratest "$S" "$W/ps1.pdf" "$W/ps1.txt" 130 259 append "x"
run --paratest "$S" "$W/ps2.pdf" "$W/ps2.txt" 130 259 append "dimensioning drawings and many more words so that the line wraps here"
run --export png "$S" "$W/ps0.png" 40; run --export png "$W/ps1.pdf" "$W/ps1.png" 40; run --export png "$W/ps2.pdf" "$W/ps2.png" 40
check "a short edit made" "grep -q 'result=ok' '$W/ps1.txt'"
check "a longer one made too (the footer far below does not stop it)" "grep -q 'result=ok' '$W/ps2.txt'"
python3 - "$W" > "$W/ps.txt" 2>&1 <<'PY'
import sys, glob
import numpy as np
from PIL import Image
w = sys.argv[1]
def png(n): return np.array(Image.open(sorted(glob.glob(f'{w}/{n}*.png'))[0]).convert('RGB')).astype(int)
a, b, c = png('ps0'), png('ps1'), png('ps2')
H, W = a.shape[:2]
def soft(x):
    p = np.pad(x, ((1, 1), (1, 1), (0, 0)), mode='edge'); s = np.zeros(x.shape)
    for dy in range(3):
        for dx in range(3): s += p[dy:dy + x.shape[0], dx:dx + x.shape[1]]
    return s / 9
def differ(x, y):   # as the app compares: smoothed, and a pixel counts only if nothing next to it in the other is close
    x, y = soft(x), soft(y)
    def far(p, q):
        qp = np.pad(q, ((1, 1), (1, 1), (0, 0)), mode='edge'); best = np.full(p.shape[:2], 1e9)
        for dy in range(3):
            for dx in range(3): best = np.minimum(best, np.abs(p - qp[dy:dy + p.shape[0], dx:dx + p.shape[1]]).max(-1))
        return best > 28
    return far(x, y) | far(y, x)
def same(x, y, box):   # box: left, top, right, bottom in pixels
    l, t, r, bo = box
    return int(differ(x, y)[t:bo, l:r].sum()) <= 4
top = (0, 0, W, int(H * 0.40)); right = (int(W * 0.57), 0, W, H)   # above the list; the card
px = lambda im: tuple(int(v) for v in im[H // 2, W // 20])
print('background', px(a), px(b), px(c))
print('short-top', same(a, b, top), 'short-right', same(a, b, right))
print('long-top', same(a, c, top), 'long-right', same(a, c, right))
PY
check "the gradient background kept (not black)" "python3 -c \"import ast,sys;l=open('$W/ps.txt').readline().split(' ',1)[1];v=ast.literal_eval('['+l.replace(') (','),(')+']');sys.exit(0 if all(min(p)>150 for p in v) else 1)\""
check "the rest of the page, symbols and shadow, unchanged" "grep -q 'short-top True short-right True' '$W/ps.txt' && grep -q 'long-top True long-right True' '$W/ps.txt'"
python3 - "$W/ps2.pdf" > "$W/ps2pos.txt" 2>&1 <<'PY'
import sys
from pdfminer.high_level import extract_pages
from pdfminer.layout import LTTextLine, LTTextContainer, LTChar
lines = []
def walk(o):
    if isinstance(o, LTTextLine): lines.append((round(o.y0, 1), round(o.x0, 1), o.get_text().strip()))
    elif hasattr(o, '__iter__'):
        for c in o: walk(c)
for p in extract_pages(sys.argv[1]): walk(p)
for l in sorted(lines, reverse=True): print(l)
PY
check "the wrapped lines start under the text, not under the bullet" "python3 -c \"
import ast
L=[ast.literal_eval(l) for l in open('$W/ps2pos.txt') if l.startswith('(')]
first=[l for l in L if 'Revolve' in l[2]][0]; f=[l for l in L if 'Fillet' in l[2]][0]; nxt=[l for l in L if f[0] < l[0] < first[0]]
import sys; sys.exit(0 if nxt and all(abs(l[1]-(first[1]+8))<1.5 for l in nxt) else 1)\""
run --blocks "$W/ps1.pdf" "$W/ps1blocks.txt"
check "edit mode offers no new pictures (the drawings set as pictures stand for drawings)" "grep -q '^pictures=0 text=7' '$W/ps1blocks.txt'"
check "the next bullet moved down, the footer stayed" "python3 -c \"
import ast,sys
L=[ast.literal_eval(l) for l in open('$W/ps2pos.txt') if l.startswith('(')]
f=[l for l in L if 'Fillet' in l[2]][0]; m=[l for l in L if l[2]=='Module 2'][0]
sys.exit(0 if f[0] < 225 and abs(m[0]-8.0)<3 else 1)\""

rm -rf "$work"
[ $fail -eq 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
