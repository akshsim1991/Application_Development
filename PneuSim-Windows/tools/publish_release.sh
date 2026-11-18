#!/usr/bin/env bash
# Publishes an installer from PneuSim-Windows/release/ to the public releases repo and updates latest.json.
# Usage: bash PneuSim-Windows/tools/publish_release.sh 3.3.5 "release notes"   (repo clone expected at /home/user/pneusim-releases)
set -e
VER="$1"; NOTES="${2:-Improvements and fixes.}"
SRC="${INSTALLER:-$(cd "$(dirname "$0")/.." && pwd)/release/PneuSim-Setup-$VER.exe}"
REPO=/home/user/pneusim-releases
[ -f "$SRC" ] || { echo "No installer for $VER in release/"; exit 1; }
SHA=$(sha256sum "$SRC" | cut -d' ' -f1)
cd "$REPO" && git pull -q --rebase origin main
cp "$SRC" "releases/PneuSim-Setup-$VER.exe"
echo "$SHA  PneuSim-Setup-$VER.exe" > "releases/PneuSim-Setup-$VER.exe.sha256"
python3 - "$VER" "$SHA" "$NOTES" <<'PY'
import json,sys,datetime
v,sha,notes=sys.argv[1:4]
json.dump({"version":v,"codename":"Mistral","released":datetime.date.today().isoformat(),
 "installerUrl":f"https://raw.githubusercontent.com/akshsim1991/PneuSim-releases/main/releases/PneuSim-Setup-{v}.exe",
 "sha256":sha,"notes":notes},open("latest.json","w"),indent=2)
PY
git add -A && git commit -qm "Release $VER" && git push -q origin HEAD:main
sleep 3
curl -s https://raw.githubusercontent.com/akshsim1991/PneuSim-releases/main/latest.json | grep '"version"'
curl -s -o /dev/null -w "installer HTTP %{http_code}, %{size_download} bytes\n" "https://raw.githubusercontent.com/akshsim1991/PneuSim-releases/main/releases/PneuSim-Setup-$VER.exe"
