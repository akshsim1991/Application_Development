#!/usr/bin/env bash
# Checks a build of the engine against the Windows app's results (golden/expected, written by
# `run.sh windows golden/expected`). Usage: check.sh PROGRAM
# A difference listed in golden/known.txt ("<file>: reason") is reported but does not fail the check.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
got="$(mktemp -d)"
bash "$here/run.sh" "$1" "$got"
fail=0
for f in "$here/golden/expected"/*; do
    b="$(basename "$f")"
    if cmp -s "$f" "$got/$b"; then echo "  same      $b"; continue; fi
    reason="$(grep "^$b:" "$here/golden/known.txt" 2>/dev/null | cut -d: -f2-)"
    if [ -n "$reason" ]; then echo "  known     $b:$reason"; continue; fi
    echo "  DIFFERENT $b"; diff "$f" "$got/$b" 2>&1 | head -8 | sed 's/^/      /'; fail=1
done
rm -rf "$got"
[ $fail -eq 0 ] && echo "ENGINE MATCHES WINDOWS" || { echo "ENGINE DIFFERS FROM WINDOWS"; exit 1; }
