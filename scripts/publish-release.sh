#!/usr/bin/env bash
# Publishes the installers in releases/ to the public releases repository (akshsim1991/FeatherPDF-releases), with the
# version file the apps read to find new versions (shared/update.hpp). Usage:
#   scripts/publish-release.sh path/to/FeatherPDF-releases-checkout "What is new, a line or two"
# It copies FeatherPDF-Setup-<version>.exe and FeatherPDF-<version>-mac.dmg, writes latest.json (version, date, notes,
# and for each file its address, size and SHA-256), then commits and pushes. Both files must be of the same version.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="$1"; notes="${2:-}"
version="$(sed -n 's/^project(FeatherPDF VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
win="FeatherPDF-Setup-$version.exe"; mac="FeatherPDF-$version-mac.dmg"
for f in "$win" "$mac"; do [ -f "$root/releases/$f" ] || { echo "missing releases/$f"; exit 1; }; done
base="https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main"
rm -f "$dest"/FeatherPDF-Setup-*.exe "$dest"/FeatherPDF-*-mac.dmg
cp "$root/releases/$win" "$root/releases/$mac" "$dest/"
python3 - "$dest" "$version" "$notes" "$base" "$win" "$mac" <<'PY'
import hashlib, json, os, sys, datetime
dest, version, notes, base, win, mac = sys.argv[1:]
def pkg(name):
    data = open(os.path.join(dest, name), 'rb').read()
    return {"url": f"{base}/{name}", "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
latest = {"version": version, "date": datetime.date.today().strftime("%-d %B %Y"), "notes": notes,
          "windows": pkg(win), "mac": pkg(mac)}
open(os.path.join(dest, "latest.json"), "w").write(json.dumps(latest, indent=2, ensure_ascii=True) + "\n")
print(json.dumps(latest, indent=2))
PY
cat > "$dest/README.md" <<MD
# Feather PDF

A fast, free PDF reader and editor for Windows and macOS. Everything happens on your own computer: no account, no
cloud service, no tracking. Free to use and share, always.

**Version $version**

- Windows 7 SP1, 8.1, 10 and 11 (64-bit): [$win]($base/$win)
- macOS 11 or later (Apple silicon and Intel): [$mac]($base/$mac)

Installed copies find new versions themselves (Help > Update to new version) and ask whether to update now or later.
MD
cd "$dest"
git add -A
[ -n "${NO_COMMIT:-}" ] && { echo "files ready in $dest (NO_COMMIT: not committed)"; exit 0; }
git commit -q -m "Feather PDF $version"
git push origin HEAD:main
