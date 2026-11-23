# Feather Image and Video Viewer (Windows)

Version 0.3: image viewer with animated GIF and a modern overlay UI. Video playback is planned.

## Download / install
Prebuilt files are in `dist/`:
- `FeatherViewer-Setup-0.3.0.exe` — installer (auto-picks 64-bit or 32-bit; Start Menu/Desktop shortcuts, "Open with" entries, uninstaller). Silent install: `/S`.
- `FeatherViewer64.exe` / `FeatherViewer32.exe` — portable, no install needed.
The installer is a plain NSIS exe (not MSI), so it does not depend on the Windows Installer service, which is unavailable in Safe Mode.

## Safe Mode design
Plain native Win32 (C++), statically linked, one `.exe`. It uses only system DLLs that exist in Safe Mode:
`user32, gdi32, gdiplus, comdlg32, shell32, shlwapi, kernel32`.
- No .NET, UWP/WinRT, DirectX/Direct2D, GPU, network, services or installer needed.
- Software (GDI) rendering works with the Safe Mode basic VGA driver.
- Portable: copy `FeatherViewer.exe` anywhere, including a USB stick.

## Features
JPEG, PNG, BMP, animated GIF (P = pause), TIFF, ICO, EMF, WMF; EXIF orientation; open dialog, drag-and-drop,
command-line path; folder browsing; zoom (wheel at cursor, +/-), fit, 100%, drag to pan, rotate, fullscreen, background colours.

| Key | Action | Key | Action |
|---|---|---|---|
| Ctrl+O | Open | Left/Right, PgUp/PgDn | Previous/next |
| Home/End | First/last | + / - / wheel | Zoom |
| 0 | Fit | 1 | Actual size |
| R / L (or Shift+R) | Rotate right / left |
| Del | Delete to Recycle Bin (asks first) | B | Cycle background |
| F or F11 | Fullscreen | Esc | Leave fullscreen |
| Double-click | Toggle fit/100% | Drag | Pan |
| Right-click | Full menu | P | Pause/resume GIF |

## Build
Linux: `sudo apt install g++-mingw-w64-x86-64-posix g++-mingw-w64-i686-posix nsis && ./build.sh` (builds 64-bit, 32-bit and the installer). Windows (MinGW-w64): run the `g++` line in `build.sh`.

## Status
Compiles cleanly (x86-64 and i686, static; installer builds). NOT yet run-tested on Windows or in Safe Mode (the build container's Wine is non-functional).
Next: test on Windows normal + Safe Mode; video (Safe Mode support for
Media Foundation/DirectShow is unreliable, so needs a bundled software decoder); slideshow, copy/delete, thumbnails.
