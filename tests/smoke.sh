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

rm -rf "$work"
[ $fail -eq 0 ] && echo "ALL PASSED" || echo "SOME FAILED"
exit $fail
