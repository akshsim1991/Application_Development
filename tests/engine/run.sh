#!/usr/bin/env bash
# Runs the engine test cases (cases.sh) on the PDFs in golden/in and writes what came out into a folder:
# <case>.rc (exit code), <case>.txt (report), <case>.note (what an edit did) and <case>.after.txt (the tables, or the
# headers and footers, found again in the PDF it wrote).
#   run.sh windows OUTDIR          with the Windows app under Wine (build/FeatherPDF.exe): writes the expected results
#   run.sh PROGRAM OUTDIR          with another build (engine_check on Linux, "Feather PDF.app/Contents/MacOS/Feather PDF")
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
. "$here/cases.sh"
prog="$1"; out="$2"
mkdir -p "$out"
tmp="$(mktemp -d)"
export WINEDEBUG=-all
path() { if [ "$prog" = windows ]; then winepath -w "$1" 2>/dev/null; else echo "$1"; fi; }
call() { if [ "$prog" = windows ]; then wine "$root/build/FeatherPDF.exe" "$@"; else "$prog" "$@"; fi; }
cases | while IFS='|' read -r name cmd; do
    [ -z "$name" ] && continue
    rm -f "$tmp"/*
    args=()
    for a in $cmd; do
        case "$a" in
            IN/*)  args+=("$(path "$here/golden/in/${a#IN/}")") ;;
            OUT/*) args+=("$(path "$tmp/${a#OUT/}")") ;;
            *)     args+=("${a//_/ }") ;;
        esac
    done
    call "${args[@]}" </dev/null >/dev/null 2>&1
    echo $? > "$out/$name.rc"
    [ -f "$tmp/r.txt" ] && tr -d '\r' < "$tmp/r.txt" > "$out/$name.txt"
    [ -f "$tmp/e.pdf.txt" ] && tr -d '\r' < "$tmp/e.pdf.txt" | sed 's/^grew \([0-9]*\.[0-9]\).*/grew \1/' > "$out/$name.note"
    if [ -f "$tmp/e.pdf" ]; then call --tables "$(path "$tmp/e.pdf")" "$(path "$tmp/a.txt")" </dev/null >/dev/null 2>&1; tr -d '\r' < "$tmp/a.txt" > "$out/$name.after.txt"; fi
    if [ -f "$tmp/h.pdf" ]; then call --hfremove "$(path "$tmp/h.pdf")" - "$(path "$tmp/a.txt")" both all </dev/null >/dev/null 2>&1; tr -d '\r' < "$tmp/a.txt" > "$out/$name.after.txt"; fi
done
rm -rf "$tmp"
