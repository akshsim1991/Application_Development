#!/usr/bin/env bash
# Publishes the installers in releases/ to the public releases repository (akshsim1991/FeatherPDF-releases), with the
# version file the apps read to find new versions (shared/update.hpp). Usage:
#   scripts/publish-release.sh path/to/FeatherPDF-releases-checkout "What is new, a line or two"
# It copies FeatherPDF-Setup-<version>.exe, FeatherPDF-<version>-mac.dmg and FeatherPDF-<version>-android.apk, writes
# latest.json (version, date, notes, and for each file its address, size and SHA-256), then commits and pushes. All of
# them must be of the same version.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="$1"; notes="${2:-}"
version="$(sed -n 's/^project(FeatherPDF VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
win="FeatherPDF-Setup-$version.exe"; mac="FeatherPDF-$version-mac.dmg"; apk="FeatherPDF-$version-android.apk"
apk32="FeatherPDF-$version-android-32bit.apk"   # older 32-bit phones (optional)
for f in "$win" "$mac" "$apk"; do [ -f "$root/releases/$f" ] || { echo "missing releases/$f"; exit 1; }; done
base="https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main"
# The previous release's files stay one release longer: GitHub's servers go on handing out the old latest.json for a few
# minutes after this is pushed, and an update started then fetches the old installer. Older ones go.
previous="$(sed -n 's/.*"version": "\([0-9.]*\)".*/\1/p' "$dest/latest.json" 2>/dev/null | head -1)"
for f in "$dest"/FeatherPDF-Setup-*.exe "$dest"/FeatherPDF-*-mac.dmg "$dest"/FeatherPDF-*-android.apk "$dest"/FeatherPDF-*-android-32bit.apk; do
  [ -e "$f" ] || continue
  case "$(basename "$f")" in *"-$version"[.-]*|*"-$version-"*) continue ;; esac
  if [ -n "$previous" ] && [ "$previous" != "$version" ]; then
    case "$(basename "$f")" in *"-$previous."*|*"-$previous-"*) continue ;; esac
  fi
  rm -f "$f"
done
cp "$root/releases/$win" "$root/releases/$mac" "$root/releases/$apk" "$dest/"
[ -f "$root/releases/$apk32" ] && cp "$root/releases/$apk32" "$dest/"
python3 - "$dest" "$version" "$notes" "$base" "$win" "$mac" "$apk" <<'PY'
import hashlib, json, os, sys, datetime
dest, version, notes, base, win, mac, apk = sys.argv[1:]
def pkg(name):
    data = open(os.path.join(dest, name), 'rb').read()
    return {"url": f"{base}/{name}", "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
latest = {"version": version, "date": datetime.date.today().strftime("%-d %B %Y"), "notes": notes,
          "windows": pkg(win), "mac": pkg(mac), "android": pkg(apk)}
open(os.path.join(dest, "latest.json"), "w").write(json.dumps(latest, indent=2, ensure_ascii=True) + "\n")
print(json.dumps(latest, indent=2))
PY
mirror="https://cdn.jsdelivr.net/gh/akshsim1991/FeatherPDF-releases@RELEASE"   # jsDelivr; RELEASE becomes the release commit once pushed
cat > "$dest/README.md" <<MD
# Feather PDF

A fast, free PDF reader and editor for Windows, macOS and Android. Everything happens on your own device: no account,
no cloud service, no tracking. Free to use and share, always.

**Version $version**

- Windows 7 SP1, 8.1, 10 and 11 (64-bit): [$win]($base/$win) (or from the mirror: [$win]($mirror/$win))
- macOS 11 or later (Apple silicon and Intel): [$mac]($base/$mac) (or from the mirror: [$mac]($mirror/$mac))
- Android 7 or later (phones and tablets): [$apk]($base/$apk) (or from the mirror: [$apk]($mirror/$apk)). Open the
  downloaded file; Android asks once whether your browser or file app may install apps. For an older phone that says
  the app is not compatible, take the 32-bit one: [$apk32]($base/$apk32) (mirror: [$apk32]($mirror/$apk32)). The first
  Android version reads, finds, edits text, highlights, draws, adds notes, turns and deletes pages, and saves.

If a download does not start, your network may block raw.githubusercontent.com (some providers do at times): use the
mirror link beside it, which serves the same file (its SHA-256 is in latest.json).

Installed copies on Windows and macOS find new versions themselves (Help > Update to new version) and ask whether to
update now or later. On Android, download the new APK and open it: it installs over the old one, keeping your settings.
MD
cd "$dest"
git add -A
[ -n "${NO_COMMIT:-}" ] && { echo "files ready in $dest (NO_COMMIT: not committed)"; exit 0; }
git commit -q -m "Feather PDF $version"
git push origin HEAD:main
# the mirror links name the commit that holds the files (jsDelivr serves a commit at once; tags cannot be pushed from every network)
sha=$(git rev-parse HEAD)
sed -i "s|FeatherPDF-releases@RELEASE|FeatherPDF-releases@$sha|g" README.md
git commit -q -am "Mirror links point at the $version release commit"
git push origin HEAD:main
