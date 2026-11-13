#!/usr/bin/env bash
# Usage: ./build.sh [test|win|all]   (default all). Win build = MinGW cross-compile -> build/FeatherSystemUtilities.exe
set -euo pipefail
cd "$(dirname "$0")"; mkdir -p build
t() { g++ -std=c++17 -Wall -Wextra -O1 -o build/core_tests tests/core_tests.cpp src/core/text.cpp && build/core_tests; }
w() {
  x86_64-w64-mingw32-windres src/win/app.rc -I src/win -O coff -o build/app.res.o
  x86_64-w64-mingw32-g++ -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -Os -municode -mwindows -static -static-libgcc -static-libstdc++ \
    -o build/FeatherSystemUtilities.exe src/win/main.cpp src/core/text.cpp build/app.res.o \
    -luuid -lcomctl32 -lshell32 -lole32 -ladvapi32 -lcomdlg32 -luser32 -lgdi32
  ls -l build/FeatherSystemUtilities.exe
}
case "${1:-all}" in test) t;; win) w;; all) t; w;; esac
