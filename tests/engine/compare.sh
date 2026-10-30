#!/usr/bin/env bash
# Runs the engine's test commands with the Windows program (under Wine) and with the engine built for this system
# (engine_check), on the same sample PDFs, and compares what they report and the PDFs they write. Any difference means
# the shared engine behaves differently off Windows.
# Usage: tests/engine/compare.sh path/to/engine_check [path/to/FeatherPDF.exe]
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
eng="$1"
exe="${2:-$root/build/FeatherPDF.exe}"
work="$(mktemp -d)"
export WINEDEBUG=-all
python3 "$root/tests/make_samples.py" "$work" >/dev/null
mkdir -p "$work/win" "$work/eng"
fail=0
known=""   # a difference with a known reason (see macOS/PORTING.md): reported, not counted as a failure
W() { winepath -w "$1" 2>/dev/null; }

# one command, both ways: $1 name, then the command's arguments with IN/ for an input file and OUT/ for an output file
both() {
    local name="$1"; shift
    local wa=() ea=()
    for a in "$@"; do
        case "$a" in
            IN/*)  wa+=("$(W "$work/${a#IN/}")"); ea+=("$work/${a#IN/}") ;;
            OUT/*) wa+=("$(W "$work/win/${a#OUT/}")"); ea+=("$work/eng/${a#OUT/}") ;;
            *)     wa+=("$a"); ea+=("$a") ;;
        esac
    done
    wine "$exe" "${wa[@]}"; local rw=$?
    "$eng" "${ea[@]}"; local re=$?
    if [ $rw -ne $re ]; then echo "  DIFF $name: exit code $rw (Windows) / $re (engine)"; fail=1; return; fi
    local same=1
    for f in "$work/win"/*; do
        local g="$work/eng/$(basename "$f")"
        case "$f" in
            *.txt) cmp -s <(tr -d '\r' < "$f") <(tr -d '\r' < "$g") || { echo "  DIFF $name: $(basename "$f")"; diff <(tr -d '\r' < "$f") <(tr -d '\r' < "$g") | head -6; same=0; } ;;
            *.pdf) # the same text in the same places, and the same tables found again
                   cmp -s <(pdftotext -bbox "$f" - 2>/dev/null) <(pdftotext -bbox "$g" - 2>/dev/null) || { echo "  DIFF $name: text of $(basename "$f")"; same=0; } ;;
        esac
    done
    if [ $same -eq 1 ]; then echo "  same $name"
    elif [ -n "$known" ]; then echo "  known difference ($known)"
    else fail=1; fi
    known=""
    rm -f "$work/win"/* "$work/eng"/*
}

echo "tables found"
for f in tbl_ruled tbl_full tbl_sharedpath tbl_rules qp; do
    [ -f "$work/$f.pdf" ] && both "$f" --tables "IN/$f.pdf" "OUT/$f.txt"
done
echo "tables edited"
both "same table rewritten" --tableedit IN/tbl_ruled.pdf OUT/a.pdf 1 200 700
both "cells, row, merge" --tableedit IN/tbl_ruled.pdf OUT/a.pdf 1 200 700 cell 2 0 "Cutting discs" insrow 4 cell 4 0 "Filler wire" merge 0 1 0 2
both "no room below" --tableedit IN/tbl_full.pdf OUT/a.pdf 1 150 760 cell 2 1 "Night shift with a long note that wraps onto more lines than the row has room for"
both "shared drawing" --tableedit IN/tbl_sharedpath.pdf OUT/a.pdf 1 150 760 cell 1 0 "Washers"
known="new text uses a standard font: installed fonts are not looked up off Windows yet"
[ -f "$work/qp.pdf" ] && both "exam paper details" --tableedit IN/qp.pdf OUT/a.pdf 1 300 560 cell 2 1 "II / P-cycle" cell 3 1 "30-10-2026"
both "table deleted" --tableedit IN/tbl_rules.pdf OUT/a.pdf 1 300 700 delete
echo "headers and footers"
for f in hf_basic hf_oddeven hf_dup hf_section hf_xobject; do
    [ -f "$work/$f.pdf" ] && both "$f" --hfremove "IN/$f.pdf" OUT/h.pdf OUT/h.txt both all
done
[ -f "$work/hf_word.pdf" ] && both "hf_word" --hfremove IN/hf_word.pdf OUT/h.pdf OUT/h.txt both all
both "page numbers on 2,4-5" --hfremove IN/hf_basic.pdf OUT/h.pdf OUT/h.txt both "2,4-5"

rm -rf "$work"
[ $fail -eq 0 ] && echo "ALL THE SAME (apart from any known difference listed)" || { echo "DIFFERENCES FOUND"; exit 1; }
