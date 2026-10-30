# Feather PDF for macOS

The Mac version of Feather PDF, version 0.2.0, on its way to everything the Windows app does: see
[PORTING.md](PORTING.md) for what is done and what comes next.

It is a native Mac app (AppKit, written in Objective-C++). It uses the same PDF engine as the Windows app (`../shared`),
so tables, headers and footers and the other tools behave the same way once their screens are there.

## What it does now

- Opens PDFs in several ways: File > Open (⌘O), Open Recent, double-clicking in Finder ("Open With" > Feather PDF),
  dragging a file onto the window or onto the Dock icon. Each document opens as a tab of the same window.
- Asks for the password of a protected PDF.
- Shows the pages one under the other, drawn sharply on Retina screens.
- Zooms with the trackpad (pinch), ⌘+ and ⌘−, ⌘0 for actual size, ⌘9 to fit the width, or the toolbar menu. A zoom
  stays on the spot under the pointer.
- Moves through the document with the toolbar's page number and ▲ ▼ buttons, the Go menu, Page Up / Page Down,
  Space, Home / End and the arrow keys.
- Page colours for reading at night (View > Page Colours: Normal, Dark, Dimmed). One setting covers every window and
  tab.
- Follows the Mac's light and dark appearance.
- A start window (Open PDF, a drop area, recent files) whenever no document is open.
- Save, Save As, Revert, and a question before closing or quitting with unsaved changes; Undo and Redo (30 steps).
- Edit PDF: watermark, page numbers, headers and footers, delete header and footer, background colour.
- Pages: rotate, insert blank pages, insert the pages of another file, delete, extract, reverse, remove blank or
  duplicate pages.
- Protect: compress, password protection (AES-256), remove hidden data.
- Export as PNG / JPEG pictures, text, Markdown, Word, Excel or CSV; save a PDF/A copy; properties; fonts used.
- The whole command line of the Windows app (`--export`, `--compress`, `--protect`, `--redact`, forms, `--pdfa`...).
- The user guide is under Help.
- Reopens the windows of the last session.

## Installing it on your Mac

1. Take `releases/FeatherPDF-<version>-mac.dmg` from the repository (on GitHub: open the file, then **Download raw file**)
   and open it.
2. Drag **Feather PDF** onto **Applications**.
3. The first time only: the app is not signed with an Apple Developer ID yet, so macOS stops it. Open it once, then go
   to **System Settings > Privacy & Security**, scroll down and click **Open Anyway** next to "Feather PDF". On
   macOS 14 and earlier, Control-click the app in Applications and choose **Open** instead.

One app runs on Apple silicon (M1 and later) and on Intel Macs, with macOS 11 Big Sur or later.

The disk image is made on GitHub's Macs each time the Mac app or the engine changes, checked (the engine's results
against the Windows app's, pages drawn, windows opened), and put into `releases/` by the build itself, next to the
Windows installer.

## Building it yourself

You need Xcode's command line tools (`xcode-select --install`) and CMake (`brew install cmake`).

```
scripts/fetch-pdfium.sh mac
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac                    # build-mac/Feather PDF.app
cmake --build build-mac --target dmg       # dist/FeatherPDF-<version>-mac.dmg
tests/engine/check.sh "$PWD/build-mac/Feather PDF.app/Contents/MacOS/Feather PDF"   # the engine against Windows' results
```

## Code

`src/main.mm` starts the app, or runs a test command and ends (`--render`, `--tables`, `--tableedit`, `--hfremove`) ·
`AppDelegate.*` the menus, page colours, help, About, and the timers that form fields need · `FPDocument.*` one open
PDF: the shared `Document` in macOS's document system · `WindowController.*` a document's window and toolbar ·
`PageView.*` the pages: layout, drawing, zoom, keys, dropping files · `Render.*` PDFium pages into macOS pictures.
