#!/usr/bin/env bash
# Publishes the installers in releases/ to the public releases repository (akshsim1991/FeatherPDF-releases), with the
# version file the apps read to find new versions (shared/update.hpp). Usage:
#   scripts/publish-release.sh path/to/FeatherPDF-releases-checkout "What is new, a line or two"
# It copies FeatherPDF-Setup-<version>.exe, FeatherPDF-<version>-mac.dmg, FeatherPDF-<version>-android.apk and
# FeatherPDF-<version>-ios.ipa, writes
# latest.json (version, date, notes, and for each file its address, size and SHA-256), then commits and pushes. All of
# them must be of the same version.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="$1"; notes="${2:-}"
version="$(sed -n 's/^project(FeatherPDF VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
win="FeatherPDF-Setup-$version.exe"; mac="FeatherPDF-$version-mac.dmg"; apk="FeatherPDF-$version-android.apk"
apk32="FeatherPDF-$version-android-32bit.apk"   # older 32-bit phones (optional)
ipa="FeatherPDF-$version-ios.ipa"   # iPhone and iPad, installed from a Mac (sideloaded)
for f in "$win" "$mac" "$apk" "$ipa"; do [ -f "$root/releases/$f" ] || { echo "missing releases/$f"; exit 1; }; done
base="https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main"
# The previous release's files stay one release longer: GitHub's servers go on handing out the old latest.json for a few
# minutes after this is pushed, and an update started then fetches the old installer. Older ones go.
previous="$(sed -n 's/.*"version": "\([0-9.]*\)".*/\1/p' "$dest/latest.json" 2>/dev/null | head -1)"
for f in "$dest"/FeatherPDF-Setup-*.exe "$dest"/FeatherPDF-*-mac.dmg "$dest"/FeatherPDF-*-android.apk "$dest"/FeatherPDF-*-android-32bit.apk "$dest"/FeatherPDF-*-ios.ipa; do
  [ -e "$f" ] || continue
  case "$(basename "$f")" in *"-$version"[.-]*|*"-$version-"*) continue ;; esac
  if [ -n "$previous" ] && [ "$previous" != "$version" ]; then
    case "$(basename "$f")" in *"-$previous."*|*"-$previous-"*) continue ;; esac
  fi
  rm -f "$f"
done
cp "$root/releases/$win" "$root/releases/$mac" "$root/releases/$apk" "$root/releases/$ipa" "$dest/"
[ -f "$root/releases/$apk32" ] && cp "$root/releases/$apk32" "$dest/"
cp "$root/docs/PRIVACY.md" "$dest/PRIVACY.md"   # the privacy policy Google Play and the apps link to
python3 - "$dest" "$version" "$notes" "$base" "$win" "$mac" "$apk" "$apk32" "$ipa" <<'PY'
import hashlib, json, os, sys, datetime
dest, version, notes, base, win, mac, apk, apk32, ipa = sys.argv[1:]
def pkg(name):
    data = open(os.path.join(dest, name), 'rb').read()
    return {"url": f"{base}/{name}", "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
latest = {"version": version, "date": datetime.date.today().strftime("%-d %B %Y"), "notes": notes,
          "windows": pkg(win), "mac": pkg(mac), "android": pkg(apk), "ios": pkg(ipa)}
if os.path.exists(os.path.join(dest, apk32)):
    latest["android32"] = pkg(apk32)   # older 32-bit phones (the Android app's updater takes it there)
open(os.path.join(dest, "latest.json"), "w").write(json.dumps(latest, indent=2, ensure_ascii=True) + "\n")
print(json.dumps(latest, indent=2))
# AltStore's source (altstore.json): with it added in AltStore (the iPhone app's Settings > Keep up to date with AltStore),
# AltStore finds and installs new versions of the iPhone app by itself
ios = latest["ios"]
app = {"name": "Feather PDF", "bundleIdentifier": "com.featherpdf.ios", "developerName": "Feather PDF",
       "subtitle": "A fast, free PDF reader and editor.",
       "localizedDescription": "Read, find, edit text and equations, highlight, draw, add notes, turn and delete pages, "
                               "and manage the PDF files on your iPhone. No account, no tracking.",
       "iconURL": f"{base}/ios-icon.png", "tintColor": "C62828",
       "version": version, "versionDate": datetime.date.today().isoformat(), "versionDescription": notes,
       "downloadURL": ios["url"], "size": ios["size"],
       "versions": [{"version": version, "date": datetime.date.today().isoformat(), "localizedDescription": notes,
                     "downloadURL": ios["url"], "size": ios["size"], "sha256": ios["sha256"], "minOSVersion": "15.0"}],
       "appPermissions": {"entitlements": [], "privacy": {}}}
source = {"name": "Feather PDF", "identifier": "com.featherpdf.source", "sourceURL": f"{base}/altstore.json",
          "website": "https://github.com/akshsim1991/FeatherPDF-releases", "apps": [app], "news": []}
open(os.path.join(dest, "altstore.json"), "w").write(json.dumps(source, indent=2, ensure_ascii=True) + "\n")
PY
cp "$root/iOS/resources/AppIcon60x60@3x.png" "$dest/ios-icon.png"   # (AltStore shows it)
mirror="https://cdn.jsdelivr.net/gh/akshsim1991/FeatherPDF-releases@RELEASE"   # jsDelivr; RELEASE becomes the release commit once pushed
cat > "$dest/README.md" <<MD
# Feather PDF

A fast, free PDF reader and editor for Windows, macOS, Android, iPhone and iPad. Everything happens on your own device: no account,
no cloud service, no tracking. Free to use and share, always.

**Version $version**

- Windows 7 SP1, 8.1, 10 and 11 (64-bit): [$win]($base/$win) (or from the mirror: [$win]($mirror/$win))
- macOS 11 or later (Apple silicon and Intel): [$mac]($base/$mac) (or from the mirror: [$mac]($mirror/$mac))
- Android 7 or later (phones and tablets): [$apk]($base/$apk) (or from the mirror: [$apk]($mirror/$apk)). Open the
  downloaded file; Android asks once whether your browser or file app may install apps. For an older phone that says
  the app is not compatible, take the 32-bit one: [$apk32]($base/$apk32) (mirror: [$apk32]($mirror/$apk32)). The first
  Android version reads, finds, edits text, highlights, draws, adds notes, turns and deletes pages, and saves.
- iPhone and iPad (iOS 15 or later): [$ipa]($base/$ipa) (or from the mirror: [$ipa]($mirror/$ipa)), installed from a
  Mac: see below.

Privacy: nothing is collected; see [PRIVACY.md](PRIVACY.md).

If a download does not start, your network may block raw.githubusercontent.com (some providers do at times): use the
mirror link beside it, which serves the same file (its SHA-256 is in latest.json).

Installed copies on Windows and macOS find new versions themselves (Help > Update to new version) and ask whether to
update now or later. So does the Android app (Settings > Advanced > Update to the new version, and a check when it opens):
it downloads the APK, checks it and hands it to Android's installer, which installs it over the old one, keeping your settings.
The iPhone app downloads a new version as soon as it is out. With AltStore on the iPhone it installs with a tap (and
AltStore can keep it up to date by itself: in the app, Settings > Keep up to date with AltStore); otherwise it is installed
from the Mac the same way as the first time (the app shares the downloaded file to the Mac with AirDrop).

## iPhone and iPad

Feather PDF for iPhone is not in the App Store: a Mac puts it on the iPhone, signed with your own Apple ID (a free
one will do).

1. Download $ipa on the Mac, and connect the iPhone to the Mac with a cable (tap Trust on the iPhone if asked).
2. Install it with **Sideloadly** (sideloadly.io): drop the .ipa on Sideloadly, type your Apple ID, press Start.
   Or with **AltStore** (altstore.io): run AltServer on the Mac, install AltStore on the iPhone from it, then open the
   .ipa in AltStore (My Apps, +).
3. On the iPhone: Settings > General > VPN & Device Management > your Apple ID > Trust. On iOS 16 or later also
   Settings > Privacy & Security > Developer Mode > On (the iPhone restarts once).

With a free Apple ID, Apple lets such an app run for 7 days; then sign it again the same way (AltStore does it by
itself when the iPhone and Mac are on the same Wi-Fi; Sideloadly has an automatic refresh too). Installing a newer
.ipa over it keeps your settings and recent documents. Open PDFs from Files, Mail or Safari with Share > Feather PDF.
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
