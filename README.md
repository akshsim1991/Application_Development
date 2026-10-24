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

Next: annotations (highlight, comments, shapes) and Save / Save As, page operations, edit text, export,
compress / password / watermark / page numbers, signing, redaction, OCR, read aloud, installer.
The fixes found in testing 2.12 (watermark resources, text-export watermark pollution, unsaved-changes
flag after Compress, small file sizes shown as 0.0 MB, clear errors when OCR or voices are missing)
land together with the features they belong to.

## Shortcuts
Ctrl+O open · Ctrl+W close tab · Ctrl+Tab next tab · Ctrl+F find · F3 / Shift+F3 next / previous match ·
Ctrl+G go to page · Ctrl+C copy · Ctrl+A select all · Ctrl+B bookmarks · Ctrl+Shift+B thumbnails ·
Ctrl+0 fit page · Ctrl+1 actual size · Ctrl+2 fit width · Ctrl +/- zoom · PgUp/PgDn/Home/End · Esc.
