#!/usr/bin/env bash
# Makes the disk image people download: the app, and a link to Applications to drag it onto.
# Usage: make-dmg.sh "path/Feather PDF.app" out.dmg
set -euo pipefail
app="$1"; out="$2"
stage="$(mktemp -d)"
cp -R "$app" "$stage/"
ln -s /Applications "$stage/Applications"
rm -f "$out"
hdiutil create -volname "Feather PDF" -srcfolder "$stage" -ov -format UDZO "$out" >/dev/null
rm -rf "$stage"
echo "$out"
