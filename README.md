# Feather PDF

A free, lightweight PDF viewer and editor for Windows. Free forever: no subscriptions, no trials, no license keys,
no nag screens, and you may share it with anyone.

Version **2.14.2** is a quality release of the 2.13 rebuild: every reported problem and every defect found while testing
is recorded with its cause, fix and test in [docs/BUGLOG.md](docs/BUGLOG.md). (2.13 was a rebuild from scratch:
the 2.12 source was lost; the old installer is kept in the repo root for reference.) The version and the build date show
in About and in the file's Properties. It is a native C++ Win32 application on top of [PDFium](https://pdfium.googlesource.com/pdfium/): one
`FeatherPDF.exe` (about 9 MB including the OCR engine) plus `pdfium.dll`, with no runtime other than Windows itself.

## Features

**Reading** · tabs in the title bar, a grouped toolbar that highlights the tool in use, page number and zoom typed into boxes, a start page with Open PDF, a drop area and recent files, a status bar (page, page size, tool, saved state), sizes that follow the display's scaling, light / dark / Windows theme, one-page / continuous / two-page layouts, back and forward, reload from disk, clickable links, side by side view (left and right or top and bottom, scrolling linked or separate, every tool and the right-click menu
on both sides; closing either file returns to one document), command palette (Ctrl+K), tabs, continuous scrolling, zoom (buttons, Ctrl+wheel, Ctrl +/−), fit width / fit page, page numbers and
Go to page, bookmarks panel, page thumbnails, search with all matches highlighted, text selection and copy, dark and
dimmed page colours, password-protected files, drag and drop, recent files, one window for every launch.

**Annotating** · highlight, underline, strikethrough, squiggly underline, comments on text or placed as notes, rectangle,
ellipse, line, arrow, freehand pen, polygon, cloud, callout with leader line, stamps with the date and time, colours and
line widths, select / edit / delete annotations, flatten (comments become part of the page; links keep working), right-click menu.

**Forms** · fill in text fields, check boxes, radio buttons, list boxes and drop-down lists; values are saved into the file.
Create text, check box, radio button, list box, drop-down and date fields; reset a form; flatten a filled-in form.

**Editing** · edit text where it is on the page: click a line (Ctrl+E) or a paragraph (F2) and type in the box that
opens over it, in the same font, size and colour; paragraphs have a style bar (font, size, bold, italic, colour,
alignment); Enter or Ctrl+Enter applies, Esc cancels, clicking elsewhere applies. Add text boxes (sans / serif / mono, size, colour), insert pictures; turn, flip, crop,
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
OCR for scanned pages (Tesseract, adds an invisible text layer; can first list the fonts used and which pages are scans), fonts used in a PDF
(type, embedded or not, subset, visible or invisible text, pages), read aloud (Windows voices), print (one window with a live
preview of every sheet, 1-9 pages per sheet, greyscale, two-sided, the printer's own settings, progress with Cancel), remove hidden data (metadata, attachments, scripts,
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
FeatherPDF.exe --fonts in.pdf report.txt                  fonts used, pages without text (scans)
```

Test modes used by `tests/smoke.sh`: `--formtest`, `--annottest`, `--pictest`, `--printsheet`.

## User guide

`docs/Feather PDF User Guide.pdf` explains the window, the toolbar icons and every feature, with screenshots and the full
shortcut and command-line lists. It is installed next to the program (F1 or … > User guide opens it). Rebuild it with
`python3 scripts/make_user_guide.py` (pictures in `docs/guide/`).

## Shortcuts

Ctrl+O open · Ctrl+W close tab · Ctrl+Tab next tab · Ctrl+S save · Ctrl+Shift+S save as · Ctrl+Z / Ctrl+Y undo / redo ·
Ctrl+F find · F3 / Shift+F3 next / previous match · Ctrl+G go to page · Ctrl+C copy · Ctrl+A select all ·
Ctrl+B bookmarks · Ctrl+Shift+B thumbnails · Ctrl+H highlight · Ctrl+U underline · Ctrl+Shift+K strikethrough ·
Ctrl+M comment · Ctrl+E edit text · Del delete selected annotation · Ctrl+P print · Ctrl+D properties ·
F1 user guide · Ctrl+Shift+U read aloud · F11 full screen · F5 presentation (click, Space or arrows for the next page) · ← → previous / next page · Ctrl+0 fit page · Ctrl+1 actual size · Ctrl+2 fit width · Ctrl+Q quit · Esc cancel.

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
wine "C:\InnoSetup\ISCC.exe" installer\FeatherPDF.iss      # or ISCC on Windows → dist/FeatherPDF-Setup-2.14.2.exe
```

Installs for the current user without an administrator prompt (or for all users on request), adds Start menu / optional
desktop shortcuts and an "Open with" entry for PDFs, and uninstalls cleanly.

## Testing

`tests/smoke.sh` runs the command line features (export, compress, protect, OCR, sign / check, paragraph editing, PDF/A,
menus) under Wine against freshly
generated sample files and checks the results with independent tools (Poppler, pikepdf, python-docx, openpyxl). Set
`VERAPDF` to the veraPDF command line to also validate the PDF/A output (`VERAPDF=/path/to/verapdf tests/smoke.sh`).
`tests/make_samples.py` creates the samples; `tests/paracheck.py` compares word positions for the editing tests.

## Code map

`src/main.cpp` window, tabs, toolbar, status bar, menus, commands · `startpage.*` the start page · `viewer.*` page view, selection, tools, form input ·
`document.*` PDFium document, undo, saving, page operations, forms · `annot.*` annotations · `pageobjects.*` watermark,
page numbers, text boxes, text and paragraph editing · `contentgen.*` writing page content without losing text spacing ·
`inlineedit.*` the edit box over the page · `picture.*` pictures, copy as picture · `redact.*` · `compress.*` ·
`protect.*` AES-256 writer · `sign.*` signatures · `ocr.*` · `exporter.*` · `speech.*` · `sidebar.*` bookmarks and thumbnails ·
`formdialog.*` reusable dialogs · `formcreate.*` / `formdata.*` creating and filling form fields · `textpdf.*` text and
Markdown to PDF · `sanitize.*` hidden data removal · `printing.*` the Print window, sheet layout and printing · `pdfa.*` PDF/A-2b · `fontlist.*` fonts report ·
`pdfedit.hpp` small raw PDF edits · `pdfium_api.*` run-time binding to pdfium.dll.

## Not in 2.14 yet

Booklet and poster printing, spell checking, PDF/UA, and new text in non-Western scripts that the document's own font
does not already contain (the built-in Latin fonts are used). Pages made by Redact become pictures. The full status of every
item of the 500-feature list is in `docs/CHECKLIST-500.md`; 2.12 parity is in `docs/PARITY.md`.

## Third-party software

PDFium and its libraries, Tesseract and Leptonica, and the tessdata_fast English model: see `THIRD_PARTY_NOTICES.md`
(all permissive licenses; the license texts ship in the `licenses/` folder).
