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

Done (milestone 1): tabs, open (dialog, drag and drop, command line, one window for all launches),
continuous view, zoom (buttons, Ctrl+wheel, Ctrl +/-/0/1/2), fit width / fit page, page navigation,
password-protected files, clear error messages. The toolbar uses text labels, so it does not depend on
icon fonts.

Next: search, bookmarks and thumbnails, annotations, saving, then the editing, export and security
features of 2.12, with the fixes found in testing 2.12 (watermark resources, text-export watermark
pollution, unsaved-changes flag after Compress, small file sizes shown as 0.0 MB, clear errors when OCR
or voices are missing).
