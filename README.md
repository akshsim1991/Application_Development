# Feather PDF

A free, lightweight PDF viewer and editor for Windows. Free forever: no subscriptions, no trials, no license keys,
no nag screens, and you may share it with anyone.

Version **2.13.0** is a rebuild from scratch (the 2.12 source was lost; the old installer is kept in the repo root for
reference). It is a native C++ Win32 application on top of [PDFium](https://pdfium.googlesource.com/pdfium/): one
`FeatherPDF.exe` (about 9 MB including the OCR engine) plus `pdfium.dll`, with no runtime other than Windows itself.

## Features

**Reading** · icon toolbar like 2.12, light / dark / Windows theme, one-page / continuous / two-page layouts, back and forward, reload from disk, clickable links, side by side view, command palette (Ctrl+K), tabs, continuous scrolling, zoom (buttons, Ctrl+wheel, Ctrl +/−), fit width / fit page, page numbers and
Go to page, bookmarks panel, page thumbnails, search with all matches highlighted, text selection and copy, dark and
dimmed page colours, password-protected files, drag and drop, recent files, one window for every launch.

**Annotating** · highlight, underline, strikethrough, squiggly underline, comments on text or placed as notes, rectangle,
ellipse, line, arrow, freehand pen, polygon, cloud, callout with leader line, stamps with the date and time, colours and
line widths, select / edit / delete annotations, flatten (comments become part of the page; links keep working), right-click menu.

**Forms** · fill in text fields, check boxes, radio buttons, list boxes and drop-down lists; values are saved into the file.
Create text, check box, radio button, list box, drop-down and date fields; reset a form; flatten a filled-in form.

**Editing** · edit existing text lines, add text boxes (sans / serif / mono, size, colour), insert pictures; turn, flip, crop,
replace, brighten / add contrast, make black and white, save as PNG and paste pictures, bring to front / send to back; undo / redo
for everything (30 steps), safe saving (temp file, then replace), unsaved-changes prompts.

**Pages** · thumbnails with multi-select, drag and a right-click menu (delete, rotate, cut / copy / paste, insert, crop, extract); delete, insert blank, rotate, move, crop (also by drawing a box), insert pages from a file, merge, split, extract, reverse the order, remove blank pages, find duplicate pages, headers and
footers (left / centre / right text with page number, page count, date, file name and title), background colour, new PDF
from pictures, new PDF from a text or Markdown file.

**Safety net** · the open tabs and their unsaved changes are saved in the background and offered again after a crash; the page you were on is remembered for each file.

**Tools** · batch mode (OCR, watermark, compress, password, signing for a whole folder, saved presets), search all PDFs in a folder, redaction check, forms (create fields, find blanks, fill from a CSV sheet, export answers), paragraph editing, move / resize pictures and text with guides, visual compare, visible signature and time stamp, find and replace, tables, bookmarks, links, handwritten signatures (type, draw or picture), ruler and protractor with units and drawing scale, compare two documents,  stamps (Approved, Draft, Confidential, custom), notes summary (all comments and highlights as Markdown), full screen
and presentation mode, watermark, page numbers, compress (recompresses large pictures), password protection with AES-256 and
permission limits (written by Feather PDF itself), digital signatures (sign with a Windows certificate or a .pfx file, make your
own certificate, check the signatures of a document, warning when a signed file was changed), redaction (mark areas / selected text / search results, then apply),
OCR for scanned pages (Tesseract, adds an invisible text layer), read aloud (Windows voices), print (preview, 1-9 pages per
sheet, greyscale, two-sided, the printer's own Properties dialog), remove hidden data (metadata, attachments, scripts,
hidden comments) with a report of what was removed, save a PDF/A-2b copy for archiving, document properties,
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
FeatherPDF.exe --sign in.pdf out.pdf cert.pfx password [timestamp-server|-] [visible]
FeatherPDF.exe --batch preset.ini                           run a saved batch preset (writes preset.ini.log)
FeatherPDF.exe --search folder text report.txt              full-text search of a folder
FeatherPDF.exe --redact in.pdf out.pdf text                 redact every occurrence and check that it is gone
FeatherPDF.exe --formdetect in.pdf out.pdf                  make fillable fields where a flat form has blanks
FeatherPDF.exe --formfill form.pdf data.csv outdir          one filled-in copy per CSV row
FeatherPDF.exe --formexport form.pdf answers.csv|xlsx
FeatherPDF.exe --visualdiff a.pdf b.pdf overlay.png [page]
FeatherPDF.exe --topdf in.txt|in.md out.pdf                 plain text or Markdown to PDF
FeatherPDF.exe --sanitize in.pdf out.pdf [comments] [bookmarks]   remove hidden data (report in out.pdf.txt)
FeatherPDF.exe --pages in.pdf out.pdf reverse|noblanks|nodupes|header|background
FeatherPDF.exe --pdfa in.pdf|in.txt|in.md out.pdf [pictures]      PDF/A-2b copy (report in out.pdf.txt)
```

Test modes used by `tests/smoke.sh`: `--formtest`, `--annottest`, `--pictest`, `--printsheet`.

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

## Testing

`tests/smoke.sh` runs the command line features (export, compress, protect, OCR, sign / check) under Wine against freshly
generated sample files and checks the results with independent tools (Poppler, pikepdf, python-docx, openpyxl). Set
`VERAPDF` to the veraPDF command line to also validate the PDF/A output (`VERAPDF=/path/to/verapdf tests/smoke.sh`).
`tests/make_samples.py` creates the samples.

## Code map

`src/main.cpp` window, tabs, toolbar, menus, commands · `viewer.*` page view, selection, tools, form input ·
`document.*` PDFium document, undo, saving, page operations, forms · `annot.*` annotations · `pageobjects.*` watermark,
page numbers, text boxes, text editing · `picture.*` pictures, copy as picture · `redact.*` · `compress.*` ·
`protect.*` AES-256 writer · `sign.*` signatures · `ocr.*` · `exporter.*` · `speech.*` · `sidebar.*` bookmarks and thumbnails ·
`formdialog.*` reusable dialogs · `formcreate.*` / `formdata.*` creating and filling form fields · `textpdf.*` text and
Markdown to PDF · `sanitize.*` hidden data removal · `printing.*` print layout and preview · `pdfa.*` PDF/A-2b ·
`pdfedit.hpp` small raw PDF edits · `pdfium_api.*` run-time binding to pdfium.dll.

## Not in 2.13 yet

Booklet and poster printing, spell checking, PDF/UA, and new or edited text in
non-Western scripts (the built-in Latin fonts are used). Pages made by Redact become pictures. The full status of every
item of the 500-feature list is in `docs/CHECKLIST-500.md`; 2.12 parity is in `docs/PARITY.md`.

## Third-party software

PDFium and its libraries, Tesseract and Leptonica, and the tessdata_fast English model: see `THIRD_PARTY_NOTICES.md`
(all permissive licenses; the license texts ship in the `licenses/` folder).
