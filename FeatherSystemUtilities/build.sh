#!/usr/bin/env bash
# Usage: ./build.sh [test|win|installer|all]   (default all). Win build = MinGW cross-compile -> build/FeatherSystemUtilities.exe
set -euo pipefail
cd "$(dirname "$0")"; mkdir -p build
t() { g++ -std=c++17 -Wall -Wextra -O1 -o build/core_tests tests/core_tests.cpp src/core/text.cpp && build/core_tests; }
w() {
  python3 tools/embed.py ui/index.html build/ui_blob.cpp
  cp -f third_party/webview2/build/native/x64/WebView2Loader.dll build/WebView2Loader.dll
  x86_64-w64-mingw32-windres src/win/app.rc -I src/win -O coff -o build/app.res.o
  x86_64-w64-mingw32-g++ -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -Wno-misleading-indentation -Wno-unused-parameter -Os -municode -mwindows -static -static-libgcc -static-libstdc++ -D_WIN32_WINNT=0x0601 -Ithird_party/webview2/build/native/include -Ithird_party/shim \
    -o build/FeatherSystemUtilities.exe src/win/main.cpp src/win/startup.cpp src/win/services.cpp src/win/inspect.cpp src/win/dialogs.cpp src/win/settings.cpp src/win/cleaner.cpp src/win/files.cpp src/win/programs.cpp src/win/update.cpp src/win/gfx.cpp src/win/quarantine.cpp src/win/webui.cpp build/ui_blob.cpp src/core/text.cpp build/app.res.o \
    -luuid -lcomctl32 -lshell32 -lole32 -loleaut32 -ladvapi32 -lcomdlg32 -luser32 -lgdi32 -lversion -lgdiplus -lgdiplus -lwinhttp -lwevtapi -lshlwapi -lpsapi -lwintrust -lcrypt32 -lbcrypt -ltaskschd -ldwmapi -luxtheme -lwbemuuid
  ls -l build/FeatherSystemUtilities.exe
}
w32() {
  python3 tools/embed.py ui/index.html build/ui_blob.cpp
  cp -f third_party/webview2/build/native/x86/WebView2Loader.dll build/WebView2Loader-x86.dll
  i686-w64-mingw32-windres -F pe-i386 src/win/app.rc -I src/win -O coff -o build/app32.res.o
  i686-w64-mingw32-g++ -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -Wno-misleading-indentation -Wno-unused-parameter -Os -municode -mwindows -static -static-libgcc -static-libstdc++ -D_WIN32_WINNT=0x0601 -Ithird_party/webview2/build/native/include -Ithird_party/shim \
    -o build/FeatherSystemUtilities-x86.exe src/win/main.cpp src/win/startup.cpp src/win/services.cpp src/win/inspect.cpp src/win/dialogs.cpp src/win/settings.cpp src/win/cleaner.cpp src/win/files.cpp src/win/programs.cpp src/win/update.cpp src/win/gfx.cpp src/win/quarantine.cpp src/win/webui.cpp build/ui_blob.cpp src/core/text.cpp build/app32.res.o \
    -luuid -lcomctl32 -lshell32 -lole32 -loleaut32 -ladvapi32 -lcomdlg32 -luser32 -lgdi32 -lversion -lgdiplus -lgdiplus -lwinhttp -lwevtapi -lshlwapi -lpsapi -lwintrust -lcrypt32 -lbcrypt -ltaskschd -ldwmapi -luxtheme -lwbemuuid
  ls -l build/FeatherSystemUtilities-x86.exe
}
i() { ( cd installer && makensis -V2 setup.nsi ) && ls -l build/FeatherSystemUtilities-Setup-*.exe; }
case "${1:-all}" in test) t;; win) w;; win32) w32;; installer) i;; all) t; w; w32; i;; esac
