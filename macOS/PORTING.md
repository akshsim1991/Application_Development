# Bringing Feather PDF to the Mac: the plan

The Windows app is about 26,000 lines. Roughly half of it is the PDF engine (`shared/`), which the Mac app uses as it
is. The other half is the Windows screens (`Windows/src`), which are written again for the Mac with AppKit, the Mac's
own toolkit, so the app looks and behaves like a Mac app.

Every stage ends with the Mac build on GitHub passing: it builds, its engine gives the Windows app's results
(`tests/engine/check.sh`), it draws pages, and it starts.

## Stage 1: foundation and viewer (done, version 0.1.0)

- The repository is split into `shared/`, `Windows/` and `macOS/`. The Windows app builds as before and all of its tests
  pass.
- The engine builds off Windows. The Windows types it used come from `shared/platform.hpp` on the Mac. Files are read and
  written with standard C. PDFium is loaded from inside the app. Form fields' timers come from the app.
- An engine check (`tests/engine`) compares 47 results (tables found and edited, headers and footers found and deleted)
  with the Windows app's. On Linux all 47 are the same. The Mac build runs it on every push. On the Mac 46 are the same
  and one is within 0.02 pt: PDFium on the Mac measures the outer edge of a 1 pt line a hair differently, so numbers
  may differ by up to 0.15 pt (`tests/engine/close.py`). Words and counts must be the same. The check found a real
  Windows bug on its first run (B-050: the letters of a "ti" ligature read the wrong way round).
- The Mac app opens and shows PDFs: tabs, zoom, pages, page colours, password, recent files and drag and drop
  ([README.md](README.md)).
- The Mac build check also opens two PDFs and has the app draw its own window into a picture, in normal and night
  colours, since GitHub's Macs do not allow screenshots of app windows.
- The night-mode page colours are one shared file (`shared/pagecolours.hpp`) for both apps.

## Engine files: where they stand on the Mac

| Engine file | On the Mac | Still to do |
|---|---|---|
| document, pageobjects, contentgen, tables, headfoot, annot, outline, redact, sanitize, formdata, formcreate, textpdf, fontlist, selftest | builds and runs | — |
| pageobjects: installed fonts | standard PDF fonts are used for edited text | find the installed font and embed it with Core Text (stage 3). Until then edited text may not match the document's font (the one known difference in the engine check) |
| protect (passwords) | says "not available yet" (`shared/pending.cpp`) | AES-256 with CommonCrypto (stage 4) |
| sign (checking and signing) | checking says "not available yet" | the Security framework (stage 4) |
| compress, exporter, picture, pdfa, visualdiff | not built | pictures through ImageIO in place of GDI+ (stage 4) |
| ocr | not built | Tesseract built for the Mac, or Apple's Vision text recognition (stage 5) |
| batch, foldersearch | not built | file listing with the standard library (stage 5) |
| speech (Windows screens) | — | AVSpeechSynthesizer (stage 5) |

## Stages to come

2. **Reading tools.** Thumbnails and bookmarks sidebar, find (⌘F) with highlighted matches, selecting and copying
   text, links, two pages side by side and single-page layouts, side-by-side documents, printing (macOS's print
   window), presentation mode.
3. **Editing.** Saving (⌘S, Save As, autosave), undo and redo, annotations (highlight, notes, drawing, shapes,
   stamps), filling forms, editing text and paragraphs with installed fonts, page operations (rotate, delete, move,
   insert, extract, merge), watermark, page numbers, header and footer, tables (find, edit, the table window), Delete
   header and footer.
4. **Security and pictures.** Passwords, redaction, signatures, compress, export (pictures, Word, Excel, text),
   PDF/A, pictures on pages, compare.
5. **The rest.** OCR, read aloud, batch, folder search, Settings window (themes, tab colours), first-run and update
   notes, a signed and notarised download (needs an Apple Developer account, about USD 99 a year; without it macOS
   asks the user to allow the app once, see README).
