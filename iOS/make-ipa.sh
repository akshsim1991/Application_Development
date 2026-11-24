#!/usr/bin/env bash
# Feather PDF.app -> FeatherPDF-<version>-ios.ipa (Payload/FeatherPDF.app, zipped): installed on an iPhone from a Mac
# with Sideloadly, AltStore or similar, which sign it with the user's Apple ID. Usage: make-ipa.sh App.app out.ipa
set -euo pipefail
app="$1"; out="$2"
tmp="$(mktemp -d)"
mkdir "$tmp/Payload"
ditto "$app" "$tmp/Payload/$(basename "$app")"
rm -f "$out"
(cd "$tmp" && zip -qry "$out" Payload)
rm -rf "$tmp"
echo "$out: $(stat -f %z "$out") bytes"
