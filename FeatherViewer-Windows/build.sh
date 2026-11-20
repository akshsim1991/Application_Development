#!/usr/bin/env bash
# Cross-compile on Linux (apt: g++-mingw-w64-x86-64-posix). On Windows with MinGW, run the same g++ line.
set -e
cd "$(dirname "$0")"
x86_64-w64-mingw32-g++ -std=c++17 -O2 -Wall -municode -mwindows -static -static-libgcc -static-libstdc++ \
  main.cpp -o FeatherViewer.exe -lgdiplus -lcomdlg32 -lshlwapi -lshell32 -lgdi32 -luser32
echo "Built FeatherViewer.exe"
