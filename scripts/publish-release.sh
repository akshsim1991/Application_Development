#!/usr/bin/env bash
# Publishes the installers in releases/ to the public releases repository (akshsim1991/FeatherPDF-releases), with the
# version file the apps read to find new versions (shared/update.hpp). Usage:
#   [PLATFORMS="ios"] scripts/publish-release.sh path/to/FeatherPDF-releases-checkout "What is new, a line or two"
# PLATFORMS says which apps this release is for (default: windows mac android ios). Each of them is copied
# (FeatherPDF-Setup-<version>.exe, FeatherPDF-<version>-mac.dmg, FeatherPDF-<version>-android.apk (and -32bit),
# FeatherPDF-<version>-ios.ipa) and gets this version and notes in latest.json (with its address, size and SHA-256);
# the others keep their entry and file as they were, so their apps are not offered an update that changes nothing.
# Then it commits and pushes.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="$1"; notes="${2:-}"
platforms="${PLATFORMS:-windows mac android ios}"
version="$(sed -n 's/^project(FeatherPDF VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
base="https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main"
cp "$root/docs/PRIVACY.md" "$dest/PRIVACY.md"   # the privacy policy Google Play and the apps link to
# latest.json, and which files each app's download is now (the shell variables for the README below)
eval "$(python3 - "$dest" "$version" "$notes" "$base" "$root/releases" "$platforms" <<'PY'
import hashlib, json, os, sys, datetime, shutil
dest, version, notes, base, rel, platforms = sys.argv[1:]
platforms = platforms.split()
names = {"windows": f"FeatherPDF-Setup-{version}.exe", "mac": f"FeatherPDF-{version}-mac.dmg",
         "android": f"FeatherPDF-{version}-android.apk", "android32": f"FeatherPDF-{version}-android-32bit.apk",
         "ios": f"FeatherPDF-{version}-ios.ipa"}
try: old = json.load(open(os.path.join(dest, "latest.json")))
except Exception: old = {}
def pkg(name):
    data = open(os.path.join(dest, name), "rb").read()
    return {"url": f"{base}/{name}", "size": len(data), "sha256": hashlib.sha256(data).hexdigest(), "version": version, "notes": notes}
def vkey(v): return [int(x) if x.isdigit() else 0 for x in v.split(".")]
latest = {}
for key in ["windows", "mac", "android", "android32", "ios"]:
    owner = "android" if key == "android32" else key
    if owner in platforms:
        src = os.path.join(rel, names[key])
        if not os.path.exists(src):
            if key == "android32": continue   # (optional)
            print(f"echo 'missing releases/{names[key]}'; exit 1"); sys.exit(0)   # (run by the shell's eval)
        shutil.copy(src, os.path.join(dest, names[key]))
        latest[key] = pkg(names[key])
    elif key in old:   # as it was (its version: its own, or the release's it came with)
        e = dict(old[key]); e.setdefault("version", old.get("version", "")); e.setdefault("notes", old.get("notes", ""))
        latest[key] = e
# The release's version, for apps that read only it (those before per-device versions): the newest of the computer
# and Android apps, so they are not offered an update that is only for other devices.
desk = [latest[k]["version"] for k in ("windows", "mac", "android") if k in latest]
top = max(desk, key=vkey) if desk else version
topNotes = next((latest[k]["notes"] for k in ("windows", "mac", "android") if k in latest and latest[k]["version"] == top), notes)
out = {"version": top, "date": datetime.date.today().strftime("%-d %B %Y"), "notes": topNotes}
for k in ["windows", "mac", "android", "ios", "android32"]:
    if k in latest: out[k] = latest[k]
open(os.path.join(dest, "latest.json"), "w").write(json.dumps(out, indent=2, ensure_ascii=True) + "\n")
print(json.dumps(out, indent=2), file=sys.stderr)
# The files: those latest.json names, and the ones the previous latest.json named (GitHub's servers go on handing out
# the old latest.json for a few minutes after a push, and an update started then fetches the old file). Others go.
keep = {os.path.basename(e["url"]) for e in out.values() if isinstance(e, dict)}
keep |= {os.path.basename(e["url"]) for e in old.values() if isinstance(e, dict) and "url" in e}
for f in os.listdir(dest):
    if f.startswith("FeatherPDF-") and f.endswith((".exe", ".dmg", ".apk", ".ipa")) and f not in keep:
        os.remove(os.path.join(dest, f))
fn = lambda k: os.path.basename(out[k]["url"]) if k in out else ""
vv = lambda k: out[k]["version"] if k in out else ""
newest = max([vv(k) for k in ("windows", "mac", "android", "ios") if vv(k)], key=vkey)
print(f'win="{fn("windows")}"; mac="{fn("mac")}"; apk="{fn("android")}"; apk32="{fn("android32")}"; ipa="{fn("ios")}"; newest="{newest}"')
print(f'vwin="{vv("windows")}"; vmac="{vv("mac")}"; vapk="{vv("android")}"; vipa="{vv("ios")}"')
PY
)"
python3 - "$dest" "$version" "$notes" "$base" <<'PY'
import json, os, sys, datetime
dest, version, notes, base = sys.argv[1:]
latest = json.load(open(os.path.join(dest, "latest.json")))
# AltStore's source (altstore.json): with it added in AltStore (the iPhone app's Settings > Keep up to date with AltStore),
# AltStore finds and installs new versions of the iPhone app by itself
ios = latest["ios"]
version, notes = ios.get("version", version), ios.get("notes", notes)   # (the iPhone app's own)
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

**Newest: version $newest** (Windows $vwin, macOS $vmac, Android $vapk, iPhone and iPad $vipa)

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
