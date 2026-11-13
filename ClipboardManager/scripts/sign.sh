#!/bin/sh
# Signs a Windows program or installer with a code-signing certificate stored in a .pfx file (osslsigncode).
#   SIGN_PFX=/path/cert.pfx SIGN_PASS='password' scripts/sign.sh file.exe [more files...]
# Optional: SIGN_TIMESTAMP (default http://timestamp.digicert.com; a signature without a timestamp stops being valid when the
# certificate expires), SIGN_NO_TIMESTAMP=1 to skip it (for offline tests only).
# Certificates issued since mid-2023 normally keep their key on a hardware token or in a cloud service and give you NO .pfx file;
# then sign on Windows with signtool instead - see docs/SIGNING.md.
set -e
: "${SIGN_PFX:?set SIGN_PFX to the .pfx certificate file (see docs/SIGNING.md)}"
TS="${SIGN_TIMESTAMP:-http://timestamp.digicert.com}"
[ "$#" -ge 1 ] || { echo "usage: scripts/sign.sh file.exe [...]"; exit 1; }
for f in "$@"; do
  tmp="$f.signing"
  if [ -n "$SIGN_NO_TIMESTAMP" ]; then
    osslsigncode sign -pkcs12 "$SIGN_PFX" -pass "${SIGN_PASS:-}" -h sha256 -n "Feather Clipboard Manager" -i "https://github.com/akshsim1991" -in "$f" -out "$tmp"
  else
    osslsigncode sign -pkcs12 "$SIGN_PFX" -pass "${SIGN_PASS:-}" -h sha256 -n "Feather Clipboard Manager" -i "https://github.com/akshsim1991" -ts "$TS" -in "$f" -out "$tmp"
  fi
  mv "$tmp" "$f"
  echo "signed: $f"
done
