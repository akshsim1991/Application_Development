#!/bin/zsh
# Builds FileGlade and prepares an update for the in-app updater (Help: FileGlade > Check for Updates).
# It creates release/FileGlade-<version>-mac.zip and release/latest.json, then prints what to publish.
# Nothing is uploaded by this script.
set -euo pipefail
cd "${0:A:h}"
./build.sh
VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" FileGlade.app/Contents/Info.plist)
OUT="release"; mkdir -p "$OUT"
ZIP="$OUT/FileGlade-$VERSION-mac.zip"
rm -f "$ZIP"
ditto -c -k --keepParent FileGlade.app "$ZIP"
SIZE=$(stat -f%z "$ZIP"); SHA=$(shasum -a 256 "$ZIP" | awk '{print $1}')
REPO_RAW="${FILEGLADE_RELEASES_RAW:-https://raw.githubusercontent.com/akshsim1991/FileGlade-releases/main}"
NOTES="${1:-FileGlade $VERSION}"
cat > "$OUT/latest.json" <<JSON
{
  "version": "$VERSION",
  "date": "$(date '+%-d %B %Y')",
  "notes": "$NOTES",
  "mac": { "url": "$REPO_RAW/FileGlade-$VERSION-mac.zip", "size": $SIZE, "sha256": "$SHA" }
}
JSON
echo "Prepared $ZIP and $OUT/latest.json"
echo "Publish: copy both files into the public FileGlade-releases repository (main branch), then commit and push."
