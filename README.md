# Feather PDF

A free, lightweight PDF viewer and editor for Windows. Free forever: no subscriptions, no trials,
no license keys, and you may share it with anyone.

`FeatherPDF-Setup-2.12.0.exe` is the previous release (its source was lost). The code in `src/` is the
rebuild, which is being brought back to feature parity and then extended (see the roadmap below).

## Build (Windows or cross-compile from Linux)

```
scripts/fetch-pdfium.sh                       # PDFium chromium/8066 (same as 2.12), into third_party/pdfium
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake      # omit the toolchain file when building with MSVC
cmake --build build                            # produces build/FeatherPDF.exe + pdfium.dll + licenses/
```

## Status of the rebuild

Done:
- **Milestone 1:** tabs, open (dialog, drag and drop, command line, one window for all launches),
  continuous view, zoom, fit width / fit page, page navigation, password-protected files, clear error
  messages, and a text-labelled toolbar that does not depend on icon fonts.
- **Milestone 2 (step 1):** text selection (drag, double-click a word, Ctrl+A) and copy; search with all
  matches highlighted, Enter / F3 for next, Shift+Enter / Shift+F3 for previous, match case, "n of N";
  bookmarks panel (Ctrl+B) and page thumbnails (Ctrl+Shift+B); Go to page (Ctrl+G or click the page
  number); application manifest (visual styles, per-monitor DPI, no UAC prompt).

- **Milestone 3:** Save (Ctrl+S) and Save As (Ctrl+Shift+S) with a safe write (temp file, then replace),
  encryption kept on password-protected files, undo / redo (Ctrl+Z, Ctrl+Y) for every change, unsaved-changes
  marker and prompts on closing, and annotations: highlight, underline, strikethrough, squiggly underline,
  comments on selected text or placed as notes, rectangle, ellipse, line, arrow and freehand pen, with colour
  and width choices, click-to-select and Delete, double-click to edit a comment, and a right-click menu.

Next: page operations (delete, insert, rotate, merge, extract, crop), edit text, export,
compress / password / watermark / page numbers, signing, redaction, OCR, read aloud, installer.
The fixes found in testing 2.12 (watermark resources, text-export watermark pollution, unsaved-changes
flag after Compress, small file sizes shown as 0.0 MB, clear errors when OCR or voices are missing)
land together with the features they belong to.

## Shortcuts
Ctrl+O open · Ctrl+W close tab · Ctrl+Tab next tab · Ctrl+F find · F3 / Shift+F3 next / previous match ·
Ctrl+G go to page · Ctrl+C copy · Ctrl+A select all · Ctrl+B bookmarks · Ctrl+Shift+B thumbnails ·
Ctrl+S save · Ctrl+Shift+S save as · Ctrl+Z / Ctrl+Y undo / redo · Ctrl+H highlight · Ctrl+U underline ·
Ctrl+Shift+K strikethrough · Ctrl+M comment · Del delete selected annotation · Ctrl+0 fit page · Ctrl+1 actual size · Ctrl+2 fit width · Ctrl +/- zoom · PgUp/PgDn/Home/End · Esc.
