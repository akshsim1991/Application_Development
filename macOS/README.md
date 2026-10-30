# Feather PDF for macOS

An early Mac version of Feather PDF, version 0.1.0. It reads PDFs. Editing comes in the next stages: see
[PORTING.md](PORTING.md) for what is done and what comes next.

It is a native Mac app (AppKit, written in Objective-C++). It uses the same PDF engine as the Windows app (`../shared`),
so tables, headers and footers and the other tools behave the same way once their screens are there.

## What it does now (stage 1)

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
- The user guide is under Help.
- Reopens the windows of the last session.

## Installing it on your Mac

The app is built on GitHub's Mac computers each time the code changes:

1. Open the repository on GitHub, then **Actions** > **macOS build**, and choose the latest run with a green tick.
2. Under **Artifacts**, download **FeatherPDF-mac**. Unzip it and open `FeatherPDF-0.1.0-mac.dmg`.
3. Drag **Feather PDF** onto **Applications**.
4. The first time only: the app is not signed with an Apple Developer ID yet, so macOS stops it. Open it once, then go
   to **System Settings > Privacy & Security**, scroll down and click **Open Anyway** next to "Feather PDF". On
   macOS 14 and earlier, Control-click the app in Applications and choose **Open** instead.

One app runs on Apple silicon (M1 and later) and on Intel Macs, with macOS 11 Big Sur or later.

## Building it yourself

You need Xcode's command line tools (`xcode-select --install`) and CMake (`brew install cmake`).

```
scripts/fetch-pdfium.sh mac
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac                    # build-mac/Feather PDF.app
cmake --build build-mac --target dmg       # dist/FeatherPDF-0.1.0-mac.dmg
tests/engine/check.sh "$PWD/build-mac/Feather PDF.app/Contents/MacOS/Feather PDF"   # the engine against Windows' results
```

## Code

`src/main.mm` starts the app, or runs a test command and ends (`--render`, `--tables`, `--tableedit`, `--hfremove`) ·
`AppDelegate.*` the menus, page colours, help, About, and the timers that form fields need · `FPDocument.*` one open
PDF: the shared `Document` in macOS's document system · `WindowController.*` a document's window and toolbar ·
`PageView.*` the pages: layout, drawing, zoom, keys, dropping files · `Render.*` PDFium pages into macOS pictures.
