#!/usr/bin/env bash
# Downloads the prebuilt PDFium (the release Feather PDF is built and tested with: chromium/8066, no V8/XFA).
#   fetch-pdfium.sh          Windows (pdfium.dll) into third_party/pdfium
#   fetch-pdfium.sh mac      macOS (libpdfium.dylib, Apple silicon and Intel in one file) into third_party/pdfium-mac
#   fetch-pdfium.sh linux    Linux (libpdfium.so, for the engine check) into third_party/pdfium-linux
#   fetch-pdfium.sh ios      iPhone (libpdfium.dylib for the phone and for the simulator) into third_party/pdfium-ios/<device|simulator>
#   fetch-pdfium.sh android  Android (libpdfium.so for arm64-v8a, armeabi-v7a and x86_64) into third_party/pdfium-android/<abi>
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "${1:-}" = ios ]; then   # iPhone and iPad: the phone's (device) and the Mac simulator's (Apple silicon)
  for pair in device:device-arm64 simulator:simulator-arm64; do
    kind=${pair%%:*}; pkg=pdfium-ios-${pair##*:}.tgz; dest=third_party/pdfium-ios/$kind
    [ -f "$dest/lib/libpdfium.dylib" ] && { echo "pdfium already present in $dest"; continue; }
    mkdir -p "$dest"; tmp="$(mktemp -d)"
    curl -fsSL --retry 3 -o "$tmp/p.tgz" "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F8066/$pkg"
    tar xzf "$tmp/p.tgz" -C "$dest"; rm -rf "$tmp"
    echo "pdfium fetched into $dest"
  done
  exit 0
fi
if [ "${1:-}" = android ]; then
  for pair in arm64-v8a:arm64 armeabi-v7a:arm x86_64:x64; do
    abi=${pair%%:*}; arch=${pair##*:}; dest=third_party/pdfium-android/$abi
    [ -f "$dest/lib/libpdfium.so" ] && { echo "pdfium already present in $dest"; continue; }
    mkdir -p "$dest"; tmp="$(mktemp -d)"
    curl -fsSL --retry 3 -o "$tmp/p.tgz" "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F8066/pdfium-android-$arch.tgz"
    tar xzf "$tmp/p.tgz" -C "$dest"; rm -rf "$tmp"
    echo "pdfium fetched into $dest"
  done
  exit 0
fi
case "${1:-win}" in
  win)   dest=third_party/pdfium;       pkg=pdfium-win-x64.tgz;  lib=bin/pdfium.dll ;;
  mac)   dest=third_party/pdfium-mac;   pkg=pdfium-mac-univ.tgz; lib=lib/libpdfium.dylib ;;
  linux) dest=third_party/pdfium-linux; pkg=pdfium-linux-x64.tgz; lib=lib/libpdfium.so ;;
  *) echo "usage: $0 [win|mac|linux|android]"; exit 2 ;;
esac
[ -f "$dest/$lib" ] && { echo "pdfium already present in $dest"; exit 0; }
mkdir -p "$dest"
tmp="$(mktemp -d)"
curl -fsSL -o "$tmp/$pkg" "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F8066/$pkg"
tar xzf "$tmp/$pkg" -C "$dest"
rm -rf "$tmp"
echo "pdfium fetched into $dest"
