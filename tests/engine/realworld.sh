#!/usr/bin/env bash
# Edits real PDFs from other programs as the app's clicks do (the --edit command of any build) and checks the result: the
# edit is made (or refused where it must be) and the text read back from the saved PDF (pdfminer) has what it should.
#   Public files: Mozilla pdf.js and py-pdf test collections on GitHub (downloaded once into $FEATHER_REALWORLD_CACHE).
#   Private files (a user's question paper, list, assignment, scanned slides): used when $FEATHER_PRIVATE_PDFS holds them
#   (ia.pdf, list.pdf, bme.pdf, m3d.pdf); never committed.
# Usage: realworld.sh PROGRAM          Needs python3 with pikepdf and pdfminer.six.
set -u
prog="$1"
cache="${FEATHER_REALWORLD_CACHE:-${TMPDIR:-/tmp}/feather-realworld}"
private="${FEATHER_PRIVATE_PDFS:-}"
work="$(mktemp -d)"
mkdir -p "$cache"
fail=0; ran=0; skipped=0
ok()  { echo "  ok   $1"; ran=$((ran + 1)); }
bad() { echo "  FAIL $1"; echo "       $2"; fail=1; ran=$((ran + 1)); }
text() { python3 -c "import sys;from pdfminer.high_level import extract_text;print(' '.join(extract_text(sys.argv[1]).split()))" "$1" 2>/dev/null; }

fetch() {   # name url [pages]: the PDF (only those pages, from 0) in the cache
  local name="$1" url="$2" pages="${3:-}"
  [ -s "$cache/$name" ] && return 0
  curl -sfL --max-time 120 -o "$cache/$name.src" "$url" || { rm -f "$cache/$name.src"; return 1; }
  if [ -n "$pages" ]; then
    python3 -c "
import pikepdf,sys
s=pikepdf.open(sys.argv[1]);o=pikepdf.new()
for p in sys.argv[3].split(','): o.pages.append(s.pages[int(p)])
o.save(sys.argv[2])" "$cache/$name.src" "$cache/$name" "$pages"
  else mv "$cache/$name.src" "$cache/$name"; fi
}
fetch gd.pdf  https://raw.githubusercontent.com/py-pdf/sample-files/main/011-google-doc-document/google-doc-document.pdf
fetch tm.pdf  https://raw.githubusercontent.com/mozilla/pdf.js/master/web/compressed.tracemonkey-pldi-09.pdf 0
fetch tam.pdf https://raw.githubusercontent.com/mozilla/pdf.js/master/test/pdfs/TAMReview.pdf 0,1
fetch at.pdf  https://raw.githubusercontent.com/mozilla/pdf.js/master/test/pdfs/alphatrans.pdf

# case name | file | edit operations | the report must match (grep -E) | the saved text must contain | must not contain
cases() {
cat <<'EOF'
gd: title, a page whose flag letters PDFium cannot write|gd.pdf|para 1 180 752 append "for testing"|para ok|Example document for testing|
gd: a picture deleted|gd.pdf|del 1 476 644|del ok picture|Example document|
gd: apostrophes stay in their words|gd.pdf|para 1 200 492 same|para ok.*it's a bad idea|it's a bad idea|
gd: find and replace a word drawn a letter at a time|gd.pdf|find better BETTER|find 8|Beautiful is BETTER than ugly|is better than
gd: italic word keeps its style when its line is set again|gd.pdf|para 1 150 512 append "they say."|para ok|right\* now. they say|
gd: table cell|gd.pdf|para 1 390 367 replace "Paris and Lyon"|para ok|Paris and Lyon|
tm: a LaTeX paragraph is the whole paragraph, justified, words joined across hyphens|tm.pdf|para 1 170 406 append "This sentence was added."|para ok lines=14 align=3.*compile than|difficult to compile than statically typed ones.*benchmark programs. This sentence was added.|
tm: a two-line centred title|tm.pdf|para 1 300 705 append "in Practice"|para ok lines=2 align=1|Dynamic Languages in Practice|
tm: find and replace through paragraphs|tm.pdf|find JavaScript ECMAScript|find 7|a dynamic compiler for ECMAScript|
tam: an abstract without its heading|tam.pdf|para 1 300 520 append "This sentence was added."|para ok lines=12|IS community. This sentence was added.|
tam: a click between two lines of a paragraph|tam.pdf|para 1 290 400 replace "Keywords: TAM."|para ok|Keywords: TAM.|
tam: text inside a form object whose letters cannot be written: refused, page unchanged|tam.pdf|para 2 300 640 same|para refused||
at: short lines keep their breaks|at.pdf|para 1 90 780 append "Edited."|para ok|Image: .5 Edited.|
at: a drawn shape moved|at.pdf|move 1 100 700 40 -30|move ok shape||
at: a drawn shape deleted|at.pdf|del 1 200 500|del ok shape||
EOF
if [ -n "$private" ]; then cat <<'EOF'
ia: a centred heading wraps centred|ia.pdf|para 1 330 722 append "AND AERONAUTICS"|para ok lines=1 align=1|ENGINEERING AND AERONAUTICS|
ia: a justified question cell keeps m³|ia.pdf|para 1 260 470 append "Also find the specific speed."|para ok.*1.1m³/s|type of turbine Also find the specific speed|
ia: a row inserted in a Word table keeps every row's text|ia.pdf|table 1 260 470 insrow 3|table ok|Derive an expression for maximum blade efficiency for single.*stage impulse turbine OR 4 In a Curtis stage|
ia: a row deleted|ia.pdf|table 1 260 470 delrow 3|table ok|Define Compounding|
ia: find and replace in table cells|ia.pdf|find turbine TURBINE|find 4|Delaval TURBINE|
bme: an exponent changed|bme.pdf|para 1 300 80 replace "b. The steady-state error when the input is r(t) = 40 + 2t + 7t³"|para ok|40 \+ 2t \+ 7t|
bme: a fraction's numerator alone|bme.pdf|para 1 317 138 replace "50(1 + 2s)"|para ok lines=1|50\(1 \+ 2s\)|
bme: a polynomial with exponents|bme.pdf|para 2 300 539 same|para ok.*s⁷ \+ 9s⁶|23s \+ 15 = 0|
m3d: scanned slide: recognised text edited visibly|m3d.pdf|para 1 2000 343 replace "LASER POWDER BED FUSION"|para ok.*scan=1|LASER POWDER BED FUSION|
EOF
fi
}

echo "== real PDFs from other programs, edited ($prog)"
while IFS='|' read -r name file ops want has hasnot; do
  [ -z "$name" ] && continue
  src="$cache/$file"; [ -n "$private" ] && [ -f "$private/$file" ] && src="$private/$file"
  if [ ! -s "$src" ]; then echo "  skip $name (no $file)"; skipped=$((skipped + 1)); continue; fi
  out="$work/$(echo "$name" | tr -c 'a-z0-9' '_').pdf"
  eval "set -- $ops"
  "$prog" --edit "$src" "$out" "$@" </dev/null >/dev/null 2>&1
  report="$(cat "$out.txt" 2>/dev/null)"
  if ! printf '%s' "$report" | grep -Eq -- "$want"; then bad "$name" "report: $report"; continue; fi
  t="$(text "$out")"
  if [ -n "$has" ] && ! printf '%s' "$t" | grep -Eq -- "$has"; then bad "$name" "text lacks: $has"; continue; fi
  if [ -n "$hasnot" ] && printf '%s' "$t" | grep -Eq -- "$hasnot"; then bad "$name" "text still has: $hasnot"; continue; fi
  ok "$name"
done < <(cases)
rm -rf "$work"
echo "$ran checked, $skipped skipped"
[ $fail -eq 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
