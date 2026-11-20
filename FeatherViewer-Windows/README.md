# Feather Image and Video Viewer (Windows)

Version 0.1: image viewer. Video playback is planned for a later version.

## Safe Mode design
Plain native Win32 (C++), statically linked, one `.exe`. It uses only system DLLs that exist in Safe Mode:
`user32, gdi32, gdiplus, comdlg32, shell32, shlwapi, kernel32`.
- No .NET, UWP/WinRT, DirectX/Direct2D, GPU, network, services or installer needed.
- Software (GDI) rendering works with the Safe Mode basic VGA driver.
- Portable: copy `FeatherViewer.exe` anywhere, including a USB stick.

## Features
JPEG, PNG, BMP, GIF (first frame), TIFF, ICO, EMF, WMF; EXIF orientation; open dialog, drag-and-drop,
command-line path; folder browsing; zoom (wheel at cursor, +/-), fit, 100%, drag to pan, rotate, fullscreen, background colours.

| Key | Action | Key | Action |
|---|---|---|---|
| Ctrl+O | Open | Left/Right, PgUp/PgDn | Previous/next |
| Home/End | First/last | + / - / wheel | Zoom |
| 0 | Fit | 1 | Actual size |
| R / Shift+R | Rotate right/left | B | Cycle background |
| F or F11 | Fullscreen | Esc | Leave fullscreen |
| Double-click | Toggle fit/100% | Drag | Pan |

## Build
Linux: `sudo apt install g++-mingw-w64-x86-64-posix && ./build.sh`. Windows (MinGW-w64): run the `g++` line in `build.sh`.

## Status
Compiles cleanly (x86-64, static). NOT yet run-tested on Windows or in Safe Mode (the build container's Wine is non-functional).
Next: test on Windows normal + Safe Mode; Win32 build (i686) for 32-bit systems; animated GIF; video (Safe Mode support for
Media Foundation/DirectShow is unreliable, so needs a bundled software decoder); slideshow, copy/delete, thumbnails.
