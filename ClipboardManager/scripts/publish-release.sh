#!/bin/sh
# Publishes the installer to the public releases repository (akshsim1991/FeatherClipboardManager-releases), the same way
# Feather PDF does: the installer is copied there and latest.json is written; running copies read latest.json and offer the update.
#   scripts/publish-release.sh path/to/FeatherClipboardManager-releases-checkout "What is new, a line or two"
set -e
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="${1:?path to a checkout of FeatherClipboardManager-releases}"
notes="${2:-}"
version=$(sed -n 's/#define APP_VERSION L"\(.*\)"/\1/p' "$root/src/app.h")
file="FeatherClipboardManager-Setup-$version.exe"
[ -f "$root/releases/$file" ] || { echo "missing releases/$file (run scripts/make_installer.sh first)"; exit 1; }
cp "$root/releases/$file" "$dest/"
sha=$(sha256sum "$root/releases/$file" | cut -d' ' -f1)
size=$(wc -c < "$root/releases/$file" | tr -d ' ')
date=$(date '+%-d %B %Y')
python3 - "$dest/latest.json" "$version" "$date" "$notes" "$file" "$sha" "$size" <<'PY'
import json, sys
path, version, date, notes, file, sha, size = sys.argv[1:]
base = "https://raw.githubusercontent.com/akshsim1991/FeatherClipboardManager-releases/main"
latest = {"version": version, "date": date, "notes": notes,
          "windows": {"url": f"{base}/{file}", "sha256": sha, "size": int(size)}}
open(path, "w").write(json.dumps(latest, indent=2, ensure_ascii=True) + "\n")
PY
cd "$dest"
git add -A
git commit -q -m "Feather Clipboard Manager $version"
git push origin HEAD:main
echo "published $version"
