#!/usr/bin/env bash
# Downloads the prebuilt PDFium (the release Feather PDF is built and tested with: chromium/8066, no V8/XFA).
#   fetch-pdfium.sh          Windows (pdfium.dll) into third_party/pdfium
#   fetch-pdfium.sh mac      macOS (libpdfium.dylib, Apple silicon and Intel in one file) into third_party/pdfium-mac
#   fetch-pdfium.sh linux    Linux (libpdfium.so, for the engine check) into third_party/pdfium-linux
set -euo pipefail
cd "$(dirname "$0")/.."
case "${1:-win}" in
  win)   dest=third_party/pdfium;       pkg=pdfium-win-x64.tgz;  lib=bin/pdfium.dll ;;
  mac)   dest=third_party/pdfium-mac;   pkg=pdfium-mac-univ.tgz; lib=lib/libpdfium.dylib ;;
  linux) dest=third_party/pdfium-linux; pkg=pdfium-linux-x64.tgz; lib=lib/libpdfium.so ;;
  *) echo "usage: $0 [win|mac|linux]"; exit 2 ;;
esac
[ -f "$dest/$lib" ] && { echo "pdfium already present in $dest"; exit 0; }
mkdir -p "$dest"
tmp="$(mktemp -d)"
curl -fsSL -o "$tmp/$pkg" "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F8066/$pkg"
tar xzf "$tmp/$pkg" -C "$dest"
rm -rf "$tmp"
echo "pdfium fetched into $dest"
