#!/bin/sh
# Builds FeatherClipboardManager.exe (64-bit Windows) with the MinGW-w64 cross compiler.
#   Ubuntu/Debian: apt-get install g++-mingw-w64-x86-64-posix
# Usage: scripts/build.sh [outdir]      (default: build/)
set -e
cd "$(dirname "$0")/.."
OUT="${1:-build}"
mkdir -p "$OUT"
CXX=x86_64-w64-mingw32-g++
RC=x86_64-w64-mingw32-windres
$RC -I src/res -i src/res/app.rc -o "$OUT/app_res.o"
$CXX -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -DNDEBUG \
  src/main.cpp src/ui.cpp src/dialogs.cpp src/imgedit.cpp src/store.cpp src/clip.cpp src/core.cpp \
  src/backup.cpp src/ocr.cpp src/updater.cpp src/selftest.cpp src/lang.cpp src/util.cpp "$OUT/app_res.o" \
  -o "$OUT/FeatherClipboardManager.exe" \
  -municode -mwindows -static -static-libgcc -static-libstdc++ \
  -lgdiplus -lwinhttp -lcomctl32 -lcomdlg32 -lshlwapi -lshell32 -luxtheme -ldwmapi -lole32 -lbcrypt -lgdi32 -luser32 -ladvapi32 -lpsapi -lversion -loleaut32 -luuid
x86_64-w64-mingw32-strip "$OUT/FeatherClipboardManager.exe"
ls -l "$OUT/FeatherClipboardManager.exe"
