# Feather PDF

A free, lightweight PDF viewer and editor for Windows. Free forever: no subscriptions, no trials, no license keys,
no nag screens, and you may share it with anyone.

Version **2.13.0** is a rebuild from scratch (the 2.12 source was lost; the old installer is kept in the repo root for
reference). It is a native C++ Win32 application on top of [PDFium](https://pdfium.googlesource.com/pdfium/): one
`FeatherPDF.exe` (about 9 MB including the OCR engine) plus `pdfium.dll`, with no runtime other than Windows itself.

## Features

**Reading** · tabs, continuous scrolling, zoom (buttons, Ctrl+wheel, Ctrl +/−), fit width / fit page, page numbers and
Go to page, bookmarks panel, page thumbnails, search with all matches highlighted, text selection and copy, dark and
dimmed page colours, password-protected files, drag and drop, recent files, one window for every launch.

**Annotating** · highlight, underline, strikethrough, squiggly underline, comments on text or placed as notes, rectangle,
ellipse, line, arrow, freehand pen, colours and line widths, select / edit / delete annotations, right-click menu.

**Forms** · fill in text fields, check boxes, radio buttons and drop-down lists; values are saved into the file.

**Editing** · edit existing text lines, add text boxes (sans / serif / mono, size, colour), insert pictures, undo / redo
for everything (30 steps), safe saving (temp file, then replace), unsaved-changes prompts.

**Pages** · delete, insert blank, rotate, move, crop, insert pages from a file, merge PDFs, extract pages to a new file.

**Tools** · stamps (Approved, Draft, Confidential, custom), notes summary (all comments and highlights as Markdown), full screen
and presentation mode, watermark, page numbers, compress (recompresses large pictures), password protection with AES-256 and
permission limits (written by Feather PDF itself), digital signatures (sign with a Windows certificate or a .pfx file, make your
own certificate, check the signatures of a document, warning when a signed file was changed), redaction (mark areas / selected text / search results, then apply),
OCR for scanned pages (Tesseract, adds an invisible text layer), read aloud (Windows voices), print, document properties,
export as PNG / JPEG pictures, plain text, Markdown, Word (.docx) and Excel (.xlsx / .csv, tables only).

**Command line** (no window; exit code 0 means success)

```
FeatherPDF.exe file.pdf [more.pdf]                          open in the running window
FeatherPDF.exe --export png|jpg|txt|md|docx|xlsx|csv in.pdf out [dpi]
FeatherPDF.exe --compress in.pdf out.pdf [dpi]
FeatherPDF.exe --protect in.pdf out.pdf open-password [owner-password]
FeatherPDF.exe --ocr in.pdf out.pdf [language]
FeatherPDF.exe --sign in.pdf out.pdf certificate.pfx password
FeatherPDF.exe --checksig file.pdf report.txt
```

## Shortcuts

Ctrl+O open · Ctrl+W close tab · Ctrl+Tab next tab · Ctrl+S save · Ctrl+Shift+S save as · Ctrl+Z / Ctrl+Y undo / redo ·
Ctrl+F find · F3 / Shift+F3 next / previous match · Ctrl+G go to page · Ctrl+C copy · Ctrl+A select all ·
Ctrl+B bookmarks · Ctrl+Shift+B thumbnails · Ctrl+H highlight · Ctrl+U underline · Ctrl+Shift+K strikethrough ·
Ctrl+M comment · Ctrl+E edit text · Del delete selected annotation · Ctrl+P print · Ctrl+D properties ·
Ctrl+Shift+U read aloud · F11 full screen · F5 presentation · Ctrl+0 fit page · Ctrl+1 actual size · Ctrl+2 fit width · Ctrl+Q quit · Esc cancel.

## Building

Builds on Windows (MSVC or MinGW) or cross-compiles from Linux with mingw-w64.

```
scripts/fetch-pdfium.sh          # PDFium chromium/8066 into third_party/pdfium (same release as 2.12)
scripts/build-tesseract.sh       # optional: OCR engine + English data into third_party/tesseract
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake      # omit the toolchain file when building with MSVC
cmake --build build              # build/FeatherPDF.exe + pdfium.dll + licenses/ (+ tessdata/ when OCR is built)
```

Without `third_party/tesseract` the app still builds; "Recognise text (OCR)" then says it is unavailable.

### Installer

```
wine "C:\InnoSetup\ISCC.exe" installer\FeatherPDF.iss      # or ISCC on Windows → dist/FeatherPDF-Setup-2.13.0.exe
```

Installs for the current user without an administrator prompt (or for all users on request), adds Start menu / optional
desktop shortcuts and an "Open with" entry for PDFs, and uninstalls cleanly.

## Code map

`src/main.cpp` window, tabs, toolbar, menus, commands · `viewer.*` page view, selection, tools, form input ·
`document.*` PDFium document, undo, saving, page operations, forms · `annot.*` annotations · `pageobjects.*` watermark,
page numbers, text boxes, text editing · `picture.*` pictures, copy as picture · `redact.*` · `compress.*` ·
`protect.*` AES-256 writer · `sign.*` signatures · `ocr.*` · `exporter.*` · `speech.*` · `sidebar.*` bookmarks and thumbnails ·
`formdialog.*` reusable dialogs · `pdfium_api.*` run-time binding to pdfium.dll.

## Not in 2.13 yet

Visible signature stamps, comparing two PDFs, side-by-side view, editing tables, adding bookmarks and links,
find and replace, measuring tools, booklet / pages-per-sheet printing, editing document properties. Text edited or added
uses the built-in Latin fonts; other scripts are not supported for new text. Pages made by Redact become pictures.

## Third-party software

PDFium and its libraries, Tesseract and Leptonica, and the tessdata_fast English model: see `THIRD_PARTY_NOTICES.md`
(all permissive licenses; the license texts ship in the `licenses/` folder).
