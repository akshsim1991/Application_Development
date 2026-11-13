#!/bin/sh
# Builds the program and the installer:  releases/FeatherClipboardManager-Setup-<version>.exe
# Needs: g++-mingw-w64-x86-64-posix, nsis, python3 + Pillow (icons).   Usage: scripts/make_installer.sh
set -e
cd "$(dirname "$0")/.."
VERSION=$(sed -n 's/#define APP_VERSION L"\(.*\)"/\1/p' src/app.h)
[ -f src/res/clipboard.ico ] || python3 scripts/make_icons.py src/res
scripts/build.sh build
mkdir -p releases
makensis -V2 -DVERSION="$VERSION" -DEXE="../build/FeatherClipboardManager.exe" -DOUT="../releases/FeatherClipboardManager-Setup-$VERSION.exe" installer/FeatherClipboardManager.nsi
ls -l "releases/FeatherClipboardManager-Setup-$VERSION.exe"
sha256sum "releases/FeatherClipboardManager-Setup-$VERSION.exe"
