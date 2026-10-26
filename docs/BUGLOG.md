# Feather PDF bug log

Every defect found in the 2.14 quality pass, with its root cause, the correction and how the correction was verified.
An entry is closed only after the fix was tested. "Verified" names the test; GUI tests are run in the Windows build under
Wine (1280 x 800 screen) by clicking with the mouse and checking screenshots, unless the entry says otherwise.

| Status | Meaning |
|---|---|
| Fixed | Corrected and verified |
| Fixed, Windows check needed | Corrected and verified under Wine; the behaviour also depends on real Windows and should be confirmed there |
| Open | Found, not yet corrected |

## Reported by the user

### B-001 Page layout "Continuous" opened Print preview, "Two pages" opened the user guide
- **Status:** Fixed (2.13, commit 68f590a)
- **Root cause:** the Print preview and User guide items were given the menu numbers that follow "Page layout", whose four
  items use four consecutive numbers (`MM_LAYOUT + 0..3`). The switch matched the new items first.
- **Fix:** `MM_PREVIEW = MM_LAYOUT + 4`; a compile-time check keeps the main menu below the recent-files range. All popup
  menus now go through `trackMenu`, and `--menucheck` builds all 12 menus with a document open and fails on any repeated
  command number inside one menu. The smoke tests run it.
- **Verified:** with the old numbering restored, `--menucheck` reports exactly these two clashes (exit 1); with the fix it
  passes. Clicking … > Page layout > Single page / Continuous / Two pages changes the layout and opens nothing else.

### B-002 Opening the … (Options) menu ran "New PDF from a text or Markdown file"
- **Status:** Fixed, Windows check needed
- **Root cause:** toolbar buttons acted on the mouse *press*. The … menu (45 entries) was taller than the screen, so
  Windows moved it up over the … button; the mouse *release* then landed on the item under the pointer — on a 1080-pixel
  screen exactly "New PDF from a text or Markdown file…" (the user's screenshot shows the menu covering the button there).
  Wine keeps the menu open on that release, so the activation itself does not happen under Wine; the overlap does.
- **Fix:** (1) toolbar buttons and the tab close buttons act on the mouse *release* over the same button, with mouse
  capture (a press dragged off the button does nothing), like Windows buttons; (2) every toolbar menu is opened with
  `TrackPopupMenuEx` and the button's rectangle as the exclusion zone (`TPM_VERTICAL`), so a menu never covers its button;
  (3) the … menu is down to 26 entries (B-003), so it fits.
- **Verified:** holding the mouse on … opens nothing; on release the menu opens entirely below the button with nothing
  activated; menu items still work (Page layout > Two pages); Edit PDF opens on a click; press-and-drag-off opens nothing;
  `--menucheck` passes (296 items).

### B-003 The … menu repeated what the toolbar and other menus already offer
- **Status:** Fixed
- **Root cause:** items were added to the … menu over time without removing older copies.
- **Fix:** removed Save, Undo, Redo (toolbar), Compare with and Side by side (Side by side button), Print preview (merged
  into Print, B-015), Read aloud and Sign with a certificate (Edit PDF menu), Select all / Copy page / Copy area (page
  right-click menu), Go to page (page box), Fit width / Fit page / Actual size (zoom box menu), Rotate left / right (Edit
  pages; Ctrl+L / Ctrl+R still work). New PDF and Go to (first, last, back, forward) became submenus. Every keyboard shortcut
  still works; every removed command is still in the menu named above and in the command palette.
- **Verified:** screenshot of the new menu; each removed command found in its other place; `--menucheck` passes.

## Found during 2.13 page-layout work (already corrected)

### B-004 The saved page layout was never applied at start
- **Status:** Fixed (commit 3902a86)
- **Root cause:** the viewers were created in `WM_CREATE`, before `loadSettings()` read the layout, so the view stayed
  "Continuous" while the menu showed the saved choice.
- **Fix:** apply the layout to both viewers right after the settings are loaded. **Verified:** start with `layout=0`.

### B-005 Single page scrolled continuously; two pages did not advance; fit page clipped spreads
- **Status:** Fixed (commit 3902a86); see that commit for the details and the screenshots in the 2.13 notes.

## 2.14 quality pass

### B-006 Zooming did not keep the point under the mouse pointer
- **Status:** Fixed
- **Root cause:** Ctrl + wheel zoomed about the middle of the window, not the pointer. Each wheel notch was one fixed
  step whatever the wheel reported, so high-resolution wheels and touchpads zoomed too fast or unevenly.
- **Fix:** `Viewer::setZoomAt` finds the page point under the pointer, applies the new zoom and scrolls so that point
  returns under the pointer. Steps of one gesture (same pointer position, under 1.5 s apart) reuse the first step's point,
  so rounding errors do not add up. The wheel step is `1.1 ^ (delta / 120)`, so a notch is 10 % and finer wheels make
  proportionally smaller steps. The zoom buttons, keys and zoom box keep the middle of the window fixed in the same way.
- **Limits (by design):** when the page is narrower than the window it is centred, so it can move sideways. At the very
  top or bottom of a document the view cannot scroll past the end, so the point can move vertically there.
- **Verified:** five.pdf scrolled to page 2, with a temporary log of the anchor point's screen position after every step.
  Twelve Ctrl + wheel steps at (636, 396) and sixteen at (226, 516), zooming 68 %–193 % in and out: the point stayed at
  the pointer's y on every step, and at its x whenever the page was wider than the window. Each zoom level returns
  exactly to the same scroll position when you zoom back to it.

### B-007 Edit paragraph broke lines early, in a different font from the page
- **Status:** Fixed
- **Root cause:** the edit box used Arial (or Times New Roman / Courier New) at the paragraph's size, whatever the PDF's
  font, and was exactly as wide as the longest line. A PDF font narrower than Arial (Calibri, for example) therefore broke
  lines words earlier than the page; the box also started a few pixels off the text, its top at the glyph tops instead of
  on the baseline, and its height did not cover the paragraph (the last line of the old text showed below it).
- **Fix:** the edit box uses the document's own font when it is installed (`fontlist::installedFace`: "ABCDEF+Calibri",
  "ArialMT", "TimesNewRomanPSMT" …). Its widths are then matched to the page: the editor measures the paragraph text as it
  will be set on the page (`pageobj::paragraphTextWidth`), narrows or widens the screen font to match, and makes up what
  rounding leaves over in the wrapping width. Lines wrap in a formatting rectangle of the page's line width (plus 0.6 %
  for whole-pixel rounding); the white box covers the whole old paragraph. The first line sits on the PDF baseline.
- **Verified:** test PDF with an embedded Calibri-compatible font (ragged, justified, and a serif paragraph): before, the
  box broke "…compressor on the / north site"; after, all three paragraphs break exactly as on the page (screenshots).

### B-008 Applying a paragraph replaced its font, moved its baseline and dropped justification
- **Status:** Fixed
- **Root cause:** every applied paragraph was set in a standard font (Helvetica, Times, Courier) and placed by the top of
  each line's glyph box, so lines without capitals moved up; the alignment always came back as "Left".
- **Fix:** `replaceParagraph` keeps the paragraph's own font, size, slant and colour when the style is unchanged, sets
  every line on the original baseline grid (origin and line step taken from the text matrices) and keeps first-line and
  hanging indents. Justified paragraphs are recognised (all lines but the last end at the same place) and set again
  justified, the extra space spread over the gaps as word spacing does. Changing the family, weight or slant uses the
  closest standard font, as before.
- **Verified:** smoke tests "unchanged text is set again word for word in place", "justified paragraph recognised / set
  again in place" (every word within 0.5 pt, see B-012), "the paragraph's own font is kept".

### B-009 New letters in a subset font: the whole paragraph changed font
- **Status:** Fixed
- **Root cause:** fonts embedded by Word and most PDF makers hold only the letters the document uses. Typing a letter the
  subset lacks made the whole paragraph fall back to Helvetica.
- **Fix:** only words with missing letters change. Such words use the installed font of the same name, embedded into the
  PDF once per page (reused by later edits; never when the font licence forbids embedding, nor for collections or CFF
  fonts); when it is not installed, the closest standard font. The edit is one undo step; a failure part-way puts the
  document back exactly (`Document::rollbackChange`).
- **Verified:** test PDF whose Calibri-compatible font holds only the 29 letters used: "Annual note:" and "Delivery is
  expected in May." come out in the same font (poppler render), the full font is embedded once (538 KB file; one
  Carlito CID font after two edits).

### B-010 A longer paragraph was drawn over the text below it
- **Status:** Fixed
- **Root cause:** the new lines simply continued downwards. Worse, the next Edit paragraph click then took the overlapping
  lines of both paragraphs for one paragraph and garbled them.
- **Fix:** what is below the paragraph in its column (text, pictures, lines, and links, highlights and fields) moves down
  by the extra lines, unless there is enough empty space anyway. When the page has no room, the change is refused with a
  message and the editor stays open; the document is unchanged.
- **Verified:** smoke tests "longer paragraph: nothing overlaps", "the next one moved down", "no room on the page: refused,
  nothing written"; GUI: three edits in a row on one page (screenshots), undo restores the original.

### B-011 Editing anything on a page shifted the words of other justified or letter-spaced lines
- **Status:** Fixed (residual below)
- **Root cause:** PDFium writes a page's content again after any object change, and writes each text object as a plain
  string: word spacing (Tw) and letter spacing (Tc) are dropped (kerning and rise are kept). Justified lines elsewhere on
  the page lost up to 20 pt at their right end; letter-spaced headings closed up. This affected every page edit (text,
  paragraphs, pictures, stamps, redaction, compression, PDF/A, forms, OCR).
- **Fix:** all page writes go through `contentgen::generate`. Text objects whose spacing would be lost are first split into
  pieces that each start exactly where their first letter is (same font, size, matrix, colours, render mode). Objects that
  cannot be split faithfully (ligatures, letters without Unicode, clipped text) stay as they are. The Select tool splits
  when it picks objects, so the picked object numbers stay valid while moving. Edit text now edits the whole line on the
  clicked baseline (B-013).
- **Residual:** PDFium measures letters with the font program's own advances, which differ from the PDF's rounded /Widths
  by a few thousandths of a point per letter; a split line can therefore be up to about 0.4 pt off at its end in viewers
  that follow /Widths exactly. Without the fix the same line moved by several points.
- **Verified:** smoke test "other text keeps word and letter spacing, kerning and rise" (to 0.05 pt) passes, and fails
  with the fix disabled ("spaced" moved 6 pt); justified paragraph next to an edited one within 0.5 pt.

### B-012 PDF/A lost kerning and spacing, and some pages became pictures needlessly
- **Status:** Fixed
- **Root cause:** fonts that are not embedded are replaced by setting each text object again as a plain string (spacing and
  kerning lost: "WA" moved 6 pt); the string came from PDFium's text, which adds spaces that are not in the object; and an
  object with no visible letters made the whole page a picture.
- **Fix:** before replacing fonts the page is split at spacing and kerning (only when every font can be replaced), trailing
  added spaces are trimmed, and text objects that draw nothing are removed instead.
- **Verified:** smoke tests "PDF/A: spaced and kerned text keeps every word in place" (0.25 pt: the replacement font's
  widths are rounded differently) and "the text reads the same"; with the previous build "WA" moved 6 pt.

### B-013 Edit text edited one PDF text object, not the line
- **Status:** Fixed
- **Root cause:** a line is often several text objects (runs, and the pieces of B-011); Edit text showed only the object
  under the pointer and set it in a standard font when a letter was missing.
- **Fix:** Edit text takes the whole line on the clicked baseline and sets it with the paragraph code on one line (same
  font, per-word fallback, same baseline). Turned (rotated) text keeps the old one-object edit.
- **Verified:** GUI: a justified line of the test PDF opens whole in the document's font, the caret where clicked; " now"
  appended; the other lines stay justified (screenshot).

### B-014 The inline editor's bar went blank; Enter did not close a message raised by Ctrl+Enter
- **Status:** Fixed
- **Root cause:** the page view had no WS_CLIPCHILDREN, so every repaint of the page drew over the editor's controls; and
  the commit ran inside the edit box's own key handling, so the message box it raised could miss the keyboard.
- **Fix:** the view clips its child windows; Enter / Ctrl+Enter post the Apply command and the commit runs after the key.
- **Verified:** GUI: refused change at the page foot: the bar stays painted, Enter closes the message, the text stays in the
  editor for shortening.

### B-015 Print and Print preview were separate; the preview came only after the settings, for the default printer
- **Status:** Fixed, Windows check needed (printing through a real Windows driver)
- **Root cause:** Print was a settings form, optionally followed by a separate preview window; "Print preview" (Ctrl+Shift+P)
  previewed one page per sheet on the default printer's paper whatever was chosen later, and a changed page range or
  layout was seen only after closing and reopening.
- **Fix:** one Print window: printer and settings on the left, every sheet as it will print on the right (scrollable,
  "Sheet 3 of 12 · pages 5-6" under each). All pages are shown at once by default; typing a page range or copies updates
  the preview a quarter of a second after the last key, any other setting at once; an invalid range is shown in red and
  Print is disabled. The paper comes from the chosen printer with the job's orientation, so the preview and the print use
  the same sheet geometry and the same layout code. Sheets are drawn when they come into view (a few are kept), so a
  1000-page document opens at once. Printing shows "Sheet n of m" with Cancel. 2 and 6 pages per sheet are laid out side
  by side on landscape paper and one above the other on portrait paper (landscape pages on portrait paper were tiny).
  Layout settings start as for an ordinary print each time; colour and sides are remembered. Ctrl+Shift+P opens the same
  window.
- **Verified:** under Wine with a CUPS PDF printer: pages 2-4 at 2 per sheet: preview and printed PDF have the same two
  sheets with the pages in the same places (screenshots); range "7-x" shows the message and disables Print; "998-" gives
  3 sheets; 1000-page document: preview opens and scrolls at once; greyscale print of 1000 sheets cancelled at sheet 11
  (button and Esc): the job is aborted, nothing printed.

### B-016 About one in ten password-protected files could not be opened
- **Status:** Fixed
- **Root cause:** an off-by-one in the PDF 2.0 password hash (ISO 32000-2 Algorithm 2.B): the loop must end when, after at
  least 64 rounds, the last byte of E is at most "rounds done - 32"; the code compared with the 0-based round index. When
  that byte fell on the boundary, Feather PDF computed one round more than every other reader, which then reported a wrong
  password or decrypted garbage. Each file uses fresh random salts, so it struck about one file in ten.
- **Fix:** the loop counts rounds done. Files written by the old code that show the problem cannot be opened by any reader;
  protect the original again.
- **Verified:** before: 2 of 20 files failed in poppler; after: 60 files × poppler (user and owner password) × qpdf, no
  failure. Smoke test "25 protected files all open with both passwords".

### B-017 Long explanations in dialogs were cut off (About, and every form with a note)
- **Status:** Fixed
- **Root cause:** a dialog note was made as tall as its number of line breaks, so a paragraph that wraps over several lines
  lost its last lines (About ended in the middle of "Free to use and share, always").
- **Fix:** the note is measured as the dialog will draw it, wrapped at the dialog's width.
- **Verified:** About shows all of its text (screenshot), with "Version 2.14.2, built 26 October 2026".

## 2.14 sweep

### B-018 Page Down and Page Up did nothing on a long page
- **Status:** Fixed (was O-002)
- **Root cause:** Page Down / Page Up meant "next / previous page", so on the last page, or in a one-page document whose page
  is taller than the window, they did nothing at all.
- **Fix:** they move one screen (90 %, so the reader keeps their place), and go to the next page's top instead when it starts
  within that screen, so pages stay aligned at "fit page". Single-page layout turns the page at the end as before.
- **Verified:** one-page document: Page Down middle → bottom → stays; Page Up back to the top. Five pages at fit page:
  page 1 → 2 → 3 → back to 2, each aligned (screenshots). Home / Ctrl+Home / End work.

### B-019 Every click beside the text with Edit text / Edit paragraph raised a message box
- **Status:** Fixed (was O-001)
- **Root cause:** a miss showed a modal message that had to be closed each time.
- **Fix:** a short notice at the bottom of the page view, which goes away by itself after a few seconds or on a click; the
  next click on text works at once.
- **Verified:** GUI: a click on empty space shows the notice; a click on text then opens the editor; the notice disappears.

### B-020 Messages pointed to menus that do not exist
- **Status:** Fixed
- **Root cause:** texts written for an earlier menu ("Tools > Digital signatures", "Tools > Password protection",
  "Fillable form > ...").
- **Fix:** they name the real places (Edit PDF > Digital signatures > Show the signatures, Edit PDF > Remove password
  protection, Edit PDF > Fillable form > ...). Every "X > Y" in the program's messages was checked against the menus.

### B-021 Recent files: same names could not be told apart, missing files stayed, "&" vanished; tests changed the list
- **Status:** Fixed
- **Root cause:** the … > Open recent submenu showed bare file names; choosing a file that no longer exists showed an error
  and kept it; "&" in a name is a menu access key ("R&D.pdf" showed as "RD.pdf"); and the --menucheck test opened its
  sample like a user, so it was added to Recent files and the settings were written.
- **Fix:** both recent-file menus use one label: the name, with the folder when another recent file has the same name, and
  "&" doubled; opening goes through one function that removes a missing file with a message. The test mode leaves
  settings, recent files and the recovery session untouched.
- **Verified:** settings file byte-identical after --menucheck; menu shows "report.pdf C:\test\a" / "report.pdf
  C:\test\b" and "R&D.pdf" (screenshot); choosing the missing one shows the message and removes it, after which the other
  "report.pdf" shows without its folder.

### B-022 Edit PDF > Edit pages: three items did nothing
- **Status:** Fixed
- **Root cause:** "New PDF from pictures", "Crop by drawing a box" and "Split into several files" were handed to the page
  commands, which had no case for them; only the command palette, which calls the functions directly, worked. The menu
  check only looked for numbers used twice, not for numbers nothing handles.
- **Fix:** the three cases; and `tests/dispatchcheck.py` (in the smoke tests) reads main.cpp and fails when a command put
  into a menu is not handled by a `case`, a comparison or a range test, following a range into the function it hands the
  command to.
- **Verified:** the check reports exactly these three with the previous code and none now; GUI: the items open the
  picture chooser, switch on the crop tool (with its instruction) and open the Split dialog. A menu nothing could open
  (the old page menu, all of whose items are in Edit pages) was removed.

### B-023 Signature pad: Esc used the signature and Enter erased it
- **Status:** Fixed
- **Root cause:** the pad's buttons were numbered 1, 2, 3, which are the numbers of OK and Cancel: Enter (OK) meant Clear and
  Esc (Cancel) meant "Use this signature". Esc with an empty pad did nothing, so the pad could not be closed from the
  keyboard (found by the menu sweep: it stayed open over every later command).
- **Fix:** the buttons have their own numbers; Esc cancels and Enter uses the signature (handled in the pad's own message
  loop, as IsDialogMessage does not do it for windows that are not dialogs everywhere); the pad takes the keyboard when it
  opens.
- **Verified:** sweep of the Annotate menu: every later item opens its own window again.

### B-024 Tools explained themselves in message boxes, or not at all
- **Status:** Fixed
- **Root cause:** inserting a picture, a stamp, a table or a signature showed a modal box with instructions; most other
  tools (crop by box, link, copy area, shapes, forms, measuring) showed nothing. "Select some text first" was a message box.
- **Fix:** every tool shows what to do in a short notice when it is switched on (one table of hints, checked against how
  each tool reads the mouse); the old message boxes became notices.
- **Verified:** sweep screenshots of the Annotate and Edit PDF menus show the notice for each tool.

### B-025 The Bookmarks panel of a document without bookmarks was blank
- **Status:** Fixed
- **Root cause:** the hint "This document has no bookmarks" was drawn behind the (empty) bookmark list.
- **Fix:** the list is hidden when it is empty, so the hint shows, with how to add a bookmark.
- **Verified:** screenshot.

### Sweep record: menus and shortcuts
Every item of every menu was chosen with the mouse and keyboard in a running copy (… menu, Edit PDF, Annotate, Comment,
Geometry, Side by side, zoom box, the page's right-click menu, the thumbnails' right-click menu: 230 items), and the
documented shortcuts were pressed one by one (36), each followed by a screenshot and a check that the program still runs.
Findings: B-021 to B-025. Greyed items (they need a selection or a second document) are covered by the GUI tests of their
features.

### B-026 Every page read stayed in memory until the document was closed
- **Status:** Fixed
- **Root cause:** loaded pages and their text were cached for reuse and never released: reading a 400-page document to the
  end used 190 MB (about 330 KB per page), growing with every page.
- **Fix:** only the 64 most recently used pages stay loaded; older ones are closed (their text first, the form engine told)
  when a new page loads, and loaded again when needed. A page whose objects the Select tool split in memory (B-011) is
  kept.
- **Verified:** 400 pages read and back: 87 MB, flat; pages reload and render; a search over all 400 pages finds all 100
  matches (poppler finds 100).

### B-027 Smaller defects found in the file sweep
- **Status:** Fixed
- An empty (0-byte) file was reported as "not a PDF document"; it now says the file is empty and may not have been saved or
  copied completely.
- When saving failed (read-only file or folder, disk full), the changes could only be kept by finding Save as: the message
  now offers to save under another name or folder at once. Verified: a read-only file, Ctrl+S, Yes, new name: saved, tab
  renamed, nothing unsaved.
- The main window and the side panel each took a screen device context to read the DPI and never gave it back (Windows has
  only a few shared ones). Found by a scan of every function for unbalanced create/delete and get/release pairs; the
  other pairs balance.
- Checked and correct: a truncated file ("damaged or has no pages"), random data ("not a PDF document"), a PDF with junk
  after its end (opens), a password-protected file (prompt, wrong password asks again, Cancel returns).

### B-028 Closing one side of Side by side left the other at half-window zoom
- **Status:** Fixed
- **Root cause:** in Side by side each document is fitted to half the window. Closing either tab ended Side by side, but
  the remaining document kept the zoom it had for half the window, so it filled less than half the full view (49% instead
  of 145% on a letter page at 1280 px).
- **Fix:** when closing a tab ends Side by side, the remaining document is fitted to the window width again.
- **Verified:** five.pdf and mixed.pdf side by side, Ctrl+W on either one: the other fills the window at fit width (145%);
  closing a third, unrelated tab leaves Side by side as it was (after B-029).

### B-029 Closing a tab to the left of the current one switched to another document
- **Status:** Fixed
- **Root cause:** after closing any tab, the tab now at the closed position was made the current one, even when the closed
  tab was not the current one. With word, five and mixed open and mixed shown, closing word showed five. In Side by side
  (mixed next to five) this put five on both sides.
- **Fix:** closing a tab that is not the current one leaves the current document on screen (only its position in the tab
  list moves); closing the current tab still shows the next one.
- **Verified:** three tabs, mixed current, click the cross on word: mixed stays, the tab bar shows five and mixed, mixed
  highlighted; Ctrl+W then shows five at fit width. In Side by side (mixed and five), closing word: mixed left, five right.
