#!/usr/bin/env bash
# Usage: tools/release.sh 3.5.0 "release notes"   (env: SIGNER="Cert subject", RELEASES_BASE=https://raw.githubusercontent.com/<owner>/<repo>/<ref>/<path>)
set -euo pipefail; cd "$(dirname "$0")/.."; V="$1"; NOTES="$2"
BASE="${RELEASES_BASE:-https://raw.githubusercontent.com/akshsim1991/FeatherSystemUtilities-releases/main}"   # then copy releases-repo/* into that repo and push
./build.sh all >/dev/null
mkdir -p releases/v$V; cp build/FeatherSystemUtilities-Setup-$V.exe releases/v$V/; cp build/FeatherSystemUtilities.exe releases/v$V/FeatherSystemUtilities-x64.exe; cp build/FeatherSystemUtilities-x86.exe build/WebView2Loader.dll build/WebView2Loader-x86.dll data/advice.json releases/v$V/
python3 tools/make_sbom.py "$V" releases/v$V/SBOM.cdx.json
printf '%s\n' "$NOTES" > releases/v$V/RELEASE_NOTES.md; (cd releases/v$V && sha256sum * > SHA256SUMS.txt)
S=$(sha256sum build/FeatherSystemUtilities-Setup-$V.exe | cut -d' ' -f1); SZ=$(stat -c %s build/FeatherSystemUtilities-Setup-$V.exe)
rm -f releases-repo/*.exe; cp build/FeatherSystemUtilities-Setup-$V.exe releases-repo/; cp data/advice.json releases-repo/advice.json
python3 - "$V" "$NOTES" "$BASE" "$S" "$SZ" "${SIGNER:-}" <<'PY'
import sys, json, datetime
v, notes, base, s, sz, signer = sys.argv[1:]
m = {"version": v, "date": datetime.date.today().strftime("%d %B %Y"), "notes": notes, "windows": {"url": f"{base}/FeatherSystemUtilities-Setup-{v}.exe", "size": int(sz), "sha256": s}}
if signer: m["signer"] = signer
json.dump(m, open("releases-repo/latest.json", "w"), indent=2, ensure_ascii=False)
PY
echo "Release $V packaged in releases/v$V and releases-repo/."
