#!/usr/bin/env bash
# Cross-compile on Linux: sudo apt install g++-mingw-w64-x86-64-posix g++-mingw-w64-i686-posix
# Output: dist/FeatherViewer64.exe (64-bit) and dist/FeatherViewer32.exe (32-bit)
set -e
cd "$(dirname "$0")"
mkdir -p dist
FLAGS="-std=c++17 -O2 -Wall -municode -mwindows -static -static-libgcc -static-libstdc++"
LIBS="-lgdiplus -lcomdlg32 -lshlwapi -lshell32 -lgdi32 -luser32"
x86_64-w64-mingw32-g++ $FLAGS main.cpp -o dist/FeatherViewer64.exe $LIBS
i686-w64-mingw32-g++   $FLAGS main.cpp -o dist/FeatherViewer32.exe $LIBS
echo "Built dist/FeatherViewer64.exe and dist/FeatherViewer32.exe"
# Installer (needs: sudo apt install nsis)
makensis -V2 installer.nsi
echo "Built dist/FeatherViewer-Setup-0.2.0.exe"
