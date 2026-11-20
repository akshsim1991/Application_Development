#!/usr/bin/env bash
# Signs a Windows binary with your code-signing certificate (Linux, needs: apt install osslsigncode).
# Usage: PFX=cert.pfx PFX_PASS=... tools/sign.sh input.exe   (the file is replaced by the signed one)
set -euo pipefail; : "${PFX:?set PFX to your .pfx file}"; : "${PFX_PASS:?set PFX_PASS}"; in="$1"; tmp="$in.signed"
osslsigncode sign -pkcs12 "$PFX" -pass "$PFX_PASS" -h sha256 -n "Feather System Utilities" -i "${SIGN_URL:-https://github.com/akshsim1991}" -t "${TIMESTAMP_URL:-http://timestamp.digicert.com}" -in "$in" -out "$tmp" && mv "$tmp" "$in"
osslsigncode verify -in "$in" | tail -5
