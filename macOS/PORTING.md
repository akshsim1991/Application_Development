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

## Stage 2: every feature of the Windows app (built, version 2.14.2)

Both apps carry the same version number (2.14.2). Every row of `docs/FEATURES-Windows-vs-macOS.md` is now on the Mac.
"Built" means: the Mac build passes, the engine and command-line checks pass on a Mac, and the window check draws the
app. Each feature is still to be tried by hand on a Mac; problems found that way go into `docs/BUGLOG.md`.

- Start window, saving, undo and redo, menus as on Windows, Settings window, auto-update.
- Reading: sidebar (thumbnails with a page menu, ⌘⌫ and dragging; bookmarks), find, text selection, links, layouts,
  side by side, printing, presentation, read aloud (AVSpeechSynthesizer), command palette, compare.
- Annotating, forms, editing (text, paragraphs, text boxes, pictures, objects, tables), page tools, redaction.
- Signatures with the Security framework (CMSEncoder, CMSDecoder, the keychain); one's own certificate; handwritten
  signatures.
- OCR with Apple's Vision; measuring; batch mode; folder search.

## Engine files: where they stand on the Mac

| Engine file | On the Mac | How |
|---|---|---|
| document, pageobjects, contentgen, tables, headfoot, annot, outline, redact, sanitize, formdata, formcreate, textpdf, fontlist, selftest, foldersearch, batch | builds and runs | standard C++ |
| pageobjects: installed fonts | edited text uses the installed font of the same name when the PDF's own copy lacks a letter | CoreText finds it; its file is embedded (TrueType; not collections or CFF fonts) |
| sign | signs and checks | the PDF part is shared; the app gives the CMS signature, the check and time stamp requests (`sign::setSystem`) |
| ocr | recognises text | the drawing of pages and the text layer are shared; the app gives the engine (`ocr::setEngine`): Vision |
| protect, compress, exporter, picture, pdfa, visualdiff | builds and runs | shared/crypto, shared/image |

## Still to come

- A signed and notarised download (needs an Apple Developer account, about USD 99 a year; without it macOS asks the
  user to allow the app once, see README).
