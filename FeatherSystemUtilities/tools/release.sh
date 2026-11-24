#!/usr/bin/env bash
# Usage: tools/release.sh 3.5.0 "release notes"   (env: SIGNER="Cert subject", RELEASES_BASE=https://raw.githubusercontent.com/<owner>/<repo>/<ref>/<path>)
set -euo pipefail; cd "$(dirname "$0")/.."; V="$1"; NOTES="$2"
BASE="${RELEASES_BASE:-https://raw.githubusercontent.com/akshsim1991/FeatherSystemUtilities-releases/main}"   # then copy releases-repo/* into that repo and push
./build.sh all >/dev/null
mkdir -p releases/v$V; cp build/FeatherSystemUtilities-Setup-$V.exe releases/v$V/; cp build/FeatherSystemUtilities.exe releases/v$V/FeatherSystemUtilities-x64.exe; cp build/FeatherSystemUtilities-x86.exe build/WebView2Loader.dll build/WebView2Loader-x86.dll data/advice.json releases/v$V/
python3 tools/make_sbom.py "$V" releases/v$V/SBOM.cdx.json
printf '%s\n' "$NOTES" > releases/v$V/RELEASE_NOTES.md; (cd releases/v$V && sha256sum * > SHA256SUMS.txt)
S=$(sha256sum build/FeatherSystemUtilities-Setup-$V.exe | cut -d' ' -f1); SZ=$(stat -c %s build/FeatherSystemUtilities-Setup-$V.exe)
KEEP=$(python3 - "$V" <<'PY'
import json, sys, os, re
try:
    m = json.load(open('releases-repo/latest.json')); print(os.path.basename(m['windows']['url']) if m['version'] != sys.argv[1] else '')
except Exception: print('')
PY
)
for x in releases-repo/*.exe; do [ -e "$x" ] || continue; [ "$(basename "$x")" = "$KEEP" ] || rm -f "$x"; done   # keep the previous installer so users can roll back to it
cp build/FeatherSystemUtilities-Setup-$V.exe releases-repo/; cp data/advice.json releases-repo/advice.json
cp releases-repo/latest.json /tmp/prev-latest.json 2>/dev/null || echo '{}' > /tmp/prev-latest.json
python3 - "$V" "$NOTES" "$BASE" "$S" "$SZ" "${SIGNER:-}" <<'PY'
import sys, json, datetime
v, notes, base, s, sz, signer = sys.argv[1:]
m = {"version": v, "date": datetime.date.today().strftime("%d %B %Y"), "notes": notes, "windows": {"url": f"{base}/FeatherSystemUtilities-Setup-{v}.exe", "size": int(sz), "sha256": s}}
if signer: m["signer"] = signer
try:
    old = json.load(open("/tmp/prev-latest.json"))
    if old.get("version") and old["version"] != v: m["previous"] = {"version": old["version"], "url": old["windows"]["url"], "size": old["windows"]["size"], "sha256": old["windows"]["sha256"]}
    elif old.get("previous"): m["previous"] = old["previous"]
except Exception: pass
import os
out = "releases-repo/latest-beta.json" if os.environ.get("CHANNEL") == "beta" else "releases-repo/latest.json"
json.dump(m, open(out, "w"), indent=2, ensure_ascii=False)
PY
echo "Release $V packaged in releases/v$V and releases-repo/."
