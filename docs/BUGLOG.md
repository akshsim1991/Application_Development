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

### B-030 A file named relative to another folder opened the wrong file, or none, in the running window
- **Status:** Fixed
- **Root cause:** a second launch hands its files to the window already open, as given on its command line. A relative
  name (`FeatherPDF five.pdf` typed in another folder) was then resolved against the running copy's folder.
- **Fix:** the second launch makes each name a full path in its own folder before handing it over.
- **Verified:** the copy before the fix, started from another folder, then `five.pdf` run from C:\test: "could not be read";
  after the fix the same steps open five.pdf in a new tab.

### B-031 A second launch waited until a message in the running window was closed
- **Status:** Fixed
- **Root cause:** the handed-over file was opened inside the message that delivers it, so any prompt (password, "not a PDF
  document", a damaged signature) kept the launching program, such as Explorer opening a file, waiting until it was
  answered.
- **Fix:** the file is remembered and opened just after the message returns.
- **Verified:** a second launch with a PNG returns at once (1 s, exit 0) while the running window shows "not a PDF
  document".

### B-032 Opening or dropping a folder said it "may have been moved, deleted, or be in use"
- **Status:** Fixed
- **Fix:** a folder is recognised and the message says so: "test" is a folder. Open a PDF document inside it. (C:\test\ with
  a trailing backslash names the folder too.)
- **Verified:** C:\test\ on the command line shows the new message; dropping goes through the same opening code
  (WM_DROPFILES calls openFile for each file), so it was checked through the command line, as Wine cannot simulate a
  drop from xdotool.
- Also checked: a PNG gives "not a PDF document"; a .md file handed over becomes a new PDF (n.pdf, heading and italics).

### B-033 A callout's text could not be changed
- **Status:** Fixed
- **Root cause:** double-click and the right-click menu offered editing only for notes and text markup. A callout (a box with
  a leader line, drawn as a picture so every viewer shows it) could only be deleted, and its menu item read "Delete this
  stamp". A spelling mistake meant deleting the callout and drawing it again.
- **Fix:** a callout now keeps where it points, where its box sits and its colour. Double-click (or right-click > Edit
  callout text...) shows the text; OK draws the callout again with the new text in the same place and colour, as one undo
  step. Emptying the text deletes the callout. The menu reads "Delete this callout". Non-Western characters are refused,
  as when drawing one. The dead line `if (cmd >= M_EDIT_COMMENT || cmd == 0) {}` was removed from the menu code.
- **Verified:** drawn "First text", double-click: the dialog shows it; new text of two lines: the box grows, the arrow still
  points at the same word, the border stays red; Ctrl+Z back to "First text", Ctrl+Y forward; saved: one Stamp with the new
  Contents; reopened after a restart: double-click edits it; emptied: removed. Smoke test "callout text changed in place"
  (one callout, new text, colour kept).
- Callouts drawn before this version keep only "Delete": where they point was not recorded.

### B-034 Closing a tab or the window during an in-place edit threw the typed text away without asking
- **Status:** Fixed
- **Root cause:** the "save the changes?" check looked only at changes already made to the document. Text typed into the
  in-place editor (Edit text, Edit paragraph) is applied only when the editor closes, which happened after the check, as
  the document was being let go: with no earlier changes the tab closed silently and the edit was lost.
- **Fix:** before the check, the edit in progress is applied, so it counts as a change. If it cannot be applied (the
  reason is shown), the editor stays open and nothing closes.
- **Verified:** five.pdf, Ctrl+E, a line, " EXTRA" typed, click the tab's cross: before the fix the tab closed and the
  start screen showed; after, the line reads "... by using EXTRA" and the save question appears; Cancel keeps the tab.
  A second line edited (" MORE"), Alt+F4: the question appears with both changes on the page; Yes: saved and closed, and
  pdftotext of the file shows both lines changed.

### B-035 During an in-place edit, Save did nothing and page actions ignored the edit
- **Status:** Fixed
- **Root cause:** the text in the in-place editor becomes a change only when the editor closes. The toolbar and tab bar
  acted on the document as it was: Save was greyed (no change yet), so a click on it did nothing and the file stayed as
  before while the user believed it saved; the page panel's menu and drag-to-reorder worked on pages while the editor
  still held text for one of them.
- **Fix:** a click anywhere on the tab bar or toolbar, a right-click on a page thumbnail and a thumbnail drag first apply
  the edit (if it cannot be applied, the reason is shown and nothing else happens); the toolbar is then redrawn so Save
  and Undo are live for that same click.
- **Verified:** five.pdf, Ctrl+E, " EXTRA" typed, one click on Save: the title loses its unsaved mark and pdftotext of
  the file shows "... by using EXTRA" (before the fix: nothing happened, the file unchanged). With the Pages panel open,
  " ZED" typed, right-click on thumbnail 1: the line is applied and the menu opens; Delete page: 4 pages; Ctrl+Z: 5 again
  with both edits.

### B-036 Text typed into a form field was not saved, and closing did not ask
- **Status:** Fixed
- **Root cause:** a field hands its text to the document only when it loses the keyboard. Ctrl+S checked for changes
  first: with only typing done there were none, so nothing was saved (the title showed no unsaved mark either); after
  other changes, the file got the field's old text while the screen showed the new. Closing the tab or the window lost
  the typing the same way, and printing showed the old value. Same family as B-034 and B-035.
- **Fix:** before saving (Ctrl+S, Save as, the toolbar), closing, printing, any click on the tab bar or toolbar and page
  panel actions, text still being typed is handed over: the in-place editor's text and the form field's. The field loses
  the keyboard as it would on a click elsewhere, so Page Down and other keys work again afterwards.
- **Verified:** form.pdf, Name field, "Jane Doe!" typed, Ctrl+S: the file's /V is "Jane Doe!" (before: unchanged,
  "Akshaya Simha"; with an earlier checkbox change: "Jane Doe" without the last character typed). Page Down then scrolls.
  " X" typed, click on the tab's cross: "Do you want to save the changes" appears with the title marked unsaved.

### B-037 Backspace did nothing in form fields, and Enter did not close a drop-down list
- **Status:** Fixed
- **Root cause:** PDFium receives Backspace and Enter as characters, but the viewer passed on only characters from the
  space upwards. A wrong letter could only be removed with Delete or by selecting it; a choice from an open drop-down list
  made with the arrow keys could not be confirmed, and multi-line fields got no new line.
- **Fix:** Backspace and Enter characters go to the field too, except with Ctrl held (Ctrl+H and Ctrl+M give the same
  codes and are shortcuts).
- **Verified:** "Akshaya Simha", End, Backspace x5, Home, Delete x2: "shaya " shown and saved. Country list opened,
  Down Down Enter: the list closes on Italy, and Italy is saved. Tab from Name moves to the checkbox, Space toggles it.

### B-038 Form fields were missing from the print preview and from the printed page
- **Status:** Fixed
- **Root cause:** PDFium's page rendering never draws form fields (widget annotations); only its form-filling drawing
  (FPDF_FFLDraw) does, and only into pictures. The viewer did both; printing did only the first, so a filled-in form
  printed blank: labels without names, ticks, choices or the fields' boxes.
- **Fix:** the preview (and greyscale printing, which prints a picture) draws the fields after the page. Colour
  printing keeps the page as sharp vector output and prints each field's box as a small picture of that part of the page
  (at most 300 dpi), placed with PDFium's own page-to-device mapping, so it lines up when the page is turned to fit the
  paper. The on-screen tint behind fields is switched off while the Print window is open; a field's own background colour
  (/MK /BG) prints, as it should.
- **Verified:** form.pdf with "PRINTME" typed into Name (B-036), Ctrl+P: the preview shows all fields (before: none);
  printed to the CUPS PDF printer: name, tick, "France" and the Pro radio button on the page, aligned at 200 dpi zoom.
  Landscape paper chosen: printed rotated with the fields in place. A two-page file (portrait page, then the form turned
  90°) on portrait paper: sheet 2 turned to fit, fields aligned in the preview and on paper.

### B-039 Exported and copied pictures carried the on-screen tint over form fields
- **Status:** Fixed
- **Root cause:** the viewer lays a light blue tint over form fields so they can be found. Export as pictures, Copy page
  / area as image, the compare view, blank and duplicate page detection, and redaction's "make the pages pictures" used the
  same drawing, so the tint was baked into their pictures. It also greyed the fields' text (about 115 instead of near
  black); with redaction it ended up permanently in the saved PDF.
- **Fix:** those pictures are drawn with the fields as they are in the file, without the tint; the screen keeps it.
- **Verified:** form.pdf exported as PNG: the Name field's background reads 204,215,255, its own /MK /BG colour (before:
  210,220,255), and its text 39,40,43 (before: about 115). Copy page as image in the running program: the clipboard
  picture shows 204,215,255; the page on screen, redrawn after zooming, still shows the tint (210,220,255).

### B-040 Reload from disk kept redaction marks and selected objects from the old content
- **Status:** Fixed
- **Root cause:** reloading replaced the document but, unlike undo, did not clear what pointed into the old pages:
  redaction marks stayed where they were and would black out whatever the new file has there when applied; objects
  picked with the Select tool stayed selected by number on pages that may now hold other objects. An in-place edit open at
  the time would have been applied to the reloaded pages.
- **Fix:** reloading clears the marks and the object selection, refreshes the side panel and search as undo does, and
  drops an in-place edit (its text is among the unsaved changes the reload throws away, after the usual question).
- **Verified:** five.pdf, a redaction mark drawn over two lines, the file replaced on disk by word.pdf, … > Reload from
  disk: the old build shows the red mark over word.pdf's text; the fixed build shows the new page with no mark. A search
  for "fusion" is run again on the new content ("No results").

### B-041 Presentation mode: arrows and clicks did not turn pages, and leaving it lost the place
- **Status:** Fixed
- **Root cause:** the program did not know a presentation was running (only that it was full screen), so slides were
  scrolled like a document: Right and Left did nothing (they did nothing outside presentations either), Down moved 60
  pixels, the wheel scrolled part of a page, a click started a text selection; Space worked only because 400 pixels
  happened to reach the next page. On leaving full screen or a presentation, the restored layout and zoom put the view
  wherever the old scroll position fell, often a blank lower part of some page.
- **Fix:** in a presentation, Right, Down, Page Down, Space and Enter go to the next page; Left, Up, Page Up, Backspace
  and Shift+Space to the previous one; a click goes forward (links and form fields still take their clicks); each wheel
  notch turns one page. Outside presentations, Left and Right turn pages. Entering or leaving full screen or a
  presentation keeps the page being read.
- **Verified:** five.pdf, F5 then Right, Right, Left, Down, click, wheel down, wheel up: pages 2, 3, 2, 3, 4, 5, 4
  (pdftotext confirms each page's first line). Right twice in the normal view: page 3; F5 starts there; Right, Esc: the
  normal view shows page 4 (before: a blank area); F11 and Esc: page 4 again. Guide and README list the keys.

### B-042 Tables could not be edited unless Feather PDF had made them
- **Status:** Fixed
- **Root cause:** Edit table and Delete table only recognised tables that Feather PDF itself had inserted (it looked for
  its own marker). A table from any other program, a table drawn with rules only, a table of lined-up text and a
  scanned table were all "no table here"; nothing showed where a table was expected to be clicked.
- **Fix:** a new table finder (`src/tables.cpp`) reads the page itself: lines that cross form a grid (merged cells where
  a line is missing), a box or rules with the columns taken from the text inside, runs of text lines whose pieces line
  up in columns, and on scanned pages the dark lines of the picture with the recognised words. Each cell keeps its text,
  font, size, bold, colour, fill and alignment. With Edit table or Delete table chosen, the tables found are outlined
  (blue for edit, red for delete); a table not found can be enclosed by dragging a box; right-click on a table offers
  Edit this table and Delete this table. The Edit table window (`src/tableeditor.cpp`) edits cells, rows, columns,
  merging, alignment, style, colours, lines and the table's place and size. Writing removes the old text and lines,
  keeps each unchanged cell's baseline and font, and moves what is below down by as much as the table grew (page
  footers stay); a table that shares text objects or drawings with other content is refused rather than damaged.
- **Verified:** smoke tests (`tests/smoke.sh`, "tables"): ruled table with a merged cell, rules-only table and lineless
  table found as 5 x 4; two-column prose not taken for a table; an unchanged rewrite reads the same; cell text, a new row
  and a merge read back as written and the old text is gone from the text layer; after lines All / Outside only / Rows
  only / None the table is found again as the same 5 x 4 table with the same alignment; delete removes text and lines;
  the scanned table is found. In the app (Wine): five.pdf page 3 outlined, click opens the editor, a cell changed and a
  row typed in, Apply, the heading below moved down keeping its spacing, Undo restores; the scanned sample edited
  ("Nuts" to "Washers") and undone; a dragged box around two columns of prose opens it as a table; Delete table with
  confirmation; right-click Edit this table.

### B-043 Edit table window: typing after a button press pressed the button again
- **Status:** Fixed
- **Root cause:** after clicking Row below (or any button) the keyboard stayed on that button, so a space typed into a
  cell pressed it again, and Tab / Enter walked through and pressed the other buttons (a test added a column and a row
  instead of typing "Filler wire").
- **Fix:** after a button or the Bold box, the keyboard goes back to the grid; lists and the position fields keep it.
- **Verified:** Row below, then typing "Filler wire", Tab, "50", Tab, "2.00", Tab, "100.00", Enter fills the new row.

### B-044 A table re-read after "Outside only" lines became one big cell
- **Status:** Fixed
- **Root cause:** with no lines inside the frame, every pair of neighbouring cells was taken as merged (a missing line
  between ruled cells means a merge). Columns taken from the text also split a column whose heading is on the left and
  whose figures are on the right, and one differently placed heading decided the whole column's alignment.
- **Fix:** merges are only read from lines where the column or row edges are drawn lines; two neighbouring text bands
  that no line has text in both of are one column; a column's alignment is the edge most of its cells share, and a
  heading that starts where the column starts stays on the left. A box around a single column of text (a framed
  paragraph) is not a table.
- **Verified:** smoke tests "lines: outside / rows / none, found again as the same table".

### Note: "Unknown font tag 'F1'" from poppler after a table rewrite
Harmless. Some generators (reportlab) leave an empty `BT ... ET` block naming a font; after the table's text is removed
PDFium drops the unused font from the page resources, and pdftotext warns about the leftover name. Viewers ignore it and
the text reads correctly.

### B-045 A toolbar tooltip could still appear over an opened menu
- **Status:** Fixed
- **Root cause:** the tooltips were switched off while a menu was open, but a tip whose delay had already started when
  the button was pressed asked for its text inside the menu's loop and was shown anyway (seen over the … menu).
- **Fix:** while a menu is open the tooltip text is empty, and a tip with no text is not shown.
- **Verified:** pointer held 1.5 s over … then clicked: the menu opens with no tip over it (before: "More: files, view,
  settings and help" sat on the menu).

## Themes, tabs and settings (requested points)
| # | Request | Done | Checked |
|---|---|---|---|
| 1 | Colourful, consistent toolbar icons in every theme, with active / hover / pressed / disabled states | Each icon has its own colour (Open amber, Save and Search blue, Undo / Redo teal, Edit violet, Annotate orange, Comment green, Geometry magenta, Side by side cyan, zoom indigo; arrows and … in the text colour), lighter on dark themes and pushed towards the text colour until it stands out 3:1 from what is behind it (toolbar, hover, the active tool's tint, pressed). Disabled icons stay grey. Settings > Appearance > Colourful toolbar icons turns it off | All seven themes; on and off |
| 2 | Random tab colours, ON / OFF | Settings > Appearance. A new tab gets one of 12 colours, one no open tab has, never the one given last; it keeps it until closed. Each theme shows the colour lighter or deeper; the name is drawn dark or white, whichever reads better. The active tab is deeper, with a strip along its foot. Off: tabs follow the theme. Saved | 3 tabs, light and dark themes; off and on |
| 3 | AMOLED Dark, Modern Light, Midnight Blue, Graphite | Four new palettes beside Light, Dark and Like Windows. Title row, tabs, toolbar, status bar, panels, menus (drawn by Feather PDF), Settings and every dialog and message box (drawn by Feather PDF), and the scroll bars of the pages and thumbnails (drawn flat by Feather PDF) take the theme; Windows' own parts (menu frames, list boxes, open / save dialogs) are asked to follow light or dark, and on Windows 11 the dialogs' title bars to take the theme's colours (these Windows features are not in Wine, so this part is untested here). Text contrast 11:1 to 19:1, secondary text 6.7:1 to 11:1 | Switched live through all seven from Settings, with three tabs open (no restart); menus, Document properties, a Yes / No question, the duplicate-PDF question and Settings in each |
| 4 | Show Tab Numbers, ON / OFF | Settings > Tabs. "1. Report.pdf"; numbers follow opening, closing and moving tabs (tabs can now be dragged along the row). Not in the window title. Saved | Tab 3 dragged to the front: 1. tbl_ruled, 2. five, 3. form; off and on |
| 5 | Settings in one place, no duplicates | One Settings window (… > Settings, Ctrl+,) with Appearance, Tabs, PDF Viewing, Editing, Performance, Advanced; switches for on / off, lists for choices, buttons for actions; each change applies at once and is saved. Removed from the … menu: Theme, Page colours, Set as default PDF viewer. The command palette opens the matching Settings group. New: open documents where last read (on / off), pages kept ready in memory, how often the recovery copy is made, clear recent files, open the settings folder, reset all | Every group shown; each switch on and off with the value read back from settings.ini; the menu check test finds no duplicate items |
| 6 | Duplicate PDF detection | Opening a file already open in a tab, by any route (Open, Explorer / second launch, drag and drop, Open recent), compares the file's identity on disk (volume and file number), so another path to the same file is recognised (other capitals, `..`, a mapped drive, a network or short name, a link). "This PDF is already open in Tab 3." with Go to Tab 3 / Cancel; the tab is never reloaded, so unsaved changes stay (the message says so when there are some). Settings > Tabs can go to the tab without asking | five.pdf opened again as `Z:\...\FIVE.pdf` and as `c:\TEST\..\test\Five.PDF`: "already open in Tab 1", then "Tab 2" after the tabs were reordered; Go to Tab 2 showed it |
| 7 | Every setting ON and OFF, every theme, no new bugs | Smoke tests pass (including the menu check for duplicate items); B-045 found and fixed | As above |

### B-046 Delete header and footer did nothing when chosen from the menu
- **Status:** Fixed
- **Root cause:** the Edit PDF menu hands its page commands to the pages handler only for numbers up to the last one
  that existed before (`cmd <= P_BGCOLOR`); the new command came after it, so the menu item closed the menu and nothing
  happened. My own test had opened the window from the command palette, which calls the handler directly, and the
  static check (`tests/dispatchcheck.py`) passed because the handler does have a `case` for it - it cannot see that
  the menu never forwards the command there.
- **Fix:** the range runs to the new command. Both header items moved from the Edit pages submenu to the Edit PDF menu
  beside Watermark and Page numbers, where they are seen. The themed menu items were made as compact as Windows' own:
  the taller ones had made the Edit PDF menu need scroll arrows on a 800-pixel screen.
- **Verified:** in the app, from the menu: Plant report.pdf, Add headers and footers (defaults: title, date, "Page n of
  4"), then Delete header and footer: the three added items found (the document's own title not), Delete, saved: no
  date or page number left on any page, the title on page 1 kept. The Edit PDF menu fits without scrolling.

### B-047 Delete header and footer: body text and headings taken for headers
- **Status:** Fixed
- **Root cause:** found by running it on real documents rather than only the generated samples: five.pdf (pages 1 and 5
  are the same page) had its title and a closing paragraph listed, because twice among three odd pages counted as an
  odd-page header; "Closing remark..." counted as a date ("remark" contains "mar"); a 16 pt heading "Section 2" on
  page 2 counted as a page number; text whose number changed from page to page counted as one repeated text.
- **Fix:** an item must be on three pages at least (every page of a two- or three-page document) and half of them, or on
  80 % of the odd (even) pages and none of the others; month names must be whole words; a page number may not be set
  larger than the document's main text size; other repeated text must be the same, numbers included.
- **Verified:** smoke tests "identical pages are not taken...", "a heading with the page's number is not a page number"
  and a Word document made by LibreOffice (header in three parts, footer "Page X of Y": all found and removed, body
  kept); five.pdf and Plant report.pdf: nothing found; the user guide: its header, rules and page numbers found.

### B-048 Editing a table in a Word-made exam paper failed: "the table got longer, and there is no room on this page"
- **Status:** Fixed
- **Root cause:** reported by the user on a Word exam paper (USN boxes, a letterhead, a details table and a questions
  table). Several faults together:
  (a) Apply rewrote every cell, untouched ones too, with Feather PDF's own padding; Word's rows are tighter than that,
  so every row "grew" even when only one date was typed, and on a full page there was no room to push the rest down.
  (b) Tables that touch (the letterhead box, the details table and the questions table) were read as one grid, so the
  "table" covered most of the page.
  (c) Word draws the borders of several tables, and the rule under the letterhead, as one drawing; a table whose border
  drawing reached outside it was refused as not editable.
  (d) When the lines were redrawn they all took one colour (the yellow border round question 3 spread to every line),
  and text typed into an empty cell came out 10 pt in a standard font.
- **Fix:** cells that were not changed keep their own text objects (only moved if their row or column moves); a row
  grows only when the edited text needs more lines than it had; when there is still no room on the page the edited
  text is set a little smaller (down to 64 %) and a notice says so, instead of refusing. Touching tables are told apart
  where their column lines differ. A shared border drawing is split: the part outside the table is kept as it was.
  When the grid is unchanged the original lines are kept; otherwise each line piece keeps its own width and colour.
  Text in an empty cell takes the size, font and colour of the nearest cell with text.
- **Verified:** smoke tests on an exam-paper lookalike made with Word-style tables (python-docx + LibreOffice): three
  tables found, letterhead excluded; empty cells filled with no row growing; a longer question grows only its row and
  the other questions keep their own letters; the yellow border stays round question 3 only; a table on a full page
  is edited with the text set smaller; one drawing shared by two tables and a rule. In the app: Edit table on the
  details table, Date typed, Apply: filled in, everything else on the page unchanged, no error.

### B-049 Side by side: the dark page colours (night mode) showed on one side only
- **Status:** Fixed
- **Root cause:** reported by the user. The page colours chosen in Settings (Normal, Dark, Dimmed) are saved and read
  back when the app starts, but the start-up code gave them only to the main view, not to the second view used by
  side by side. Changing the setting while the app ran set both views, so it showed only after a restart: the left
  side dark, the right side white. The page memory size had the same omission.
- **Fix:** at start-up both views get the saved page colours and page memory size.
- **Verified:** in the app, with Dark page colours saved: restarted, opened two files, Side by side, Show another open
  tab: both sides dark (before the fix the right side was white).

### B-050 Tables: the two letters of a ligature could be read the wrong way round ("representaiton")
- **Status:** Fixed
- **Root cause:** found by the new Mac build check, which compares the engine on the Mac with the Windows app. Word
  draws "ti" as one ligature glyph, so PDFium reports the "t" and the "i" at the same place. The table reader sorted
  the letters of a line only by their place, so two letters at the same place could come out in either order. The
  Windows app's C++ library put the "i" first, and the exam paper's question cell read "representaiton". When such a cell
  was edited, the wrong spelling was written back. The Mac's library happened to put them right, which showed the
  fault.
- **Fix:** letters at the same place keep the order the PDF gives them. The same tie-break was added to the other
  sorts by position (lines of a table, objects of a paragraph), so the result never depends on the system.
- **Verified:** smoke test "exam paper: the letters of a ligature ("ti") read in order"; the engine check now gives the
  same 47 results on Windows, Linux and the Mac.

## Delete Header and Footer (requested points)
Engine `src/headfoot.cpp`, window `src/hfdialog.cpp`; command line `--hfremove` for the smoke tests ("delete header and
footer", 15 checks on generated samples: a report with logo / title / date / rules / "Page n of 6" and body text that
starts inside the top strip, a book with odd / even headers and outer page numbers, shared form XObjects, scanned pages,
a single page).

| Request | Done | Checked |
|---|---|---|
| Automatic detection: repeated text, page numbers, dates, titles, logos, lines | Objects wholly inside the top or bottom 16 % of the page that repeat in the same place on at least half the pages (or of the odd, or of the even pages); pieces of a line of text are joined first. Page numbers: a short label ("3", "- 3 -", "Page 3 of 6", "iv") whose number follows the page on 80 % of the pages. Dates, text, pictures, lines, shapes and whole forms are named as such. Tagged PDFs: objects marked as header, footer or pagination artifacts count too | Smoke tests: exactly the 6 items of the report; "odd pages" / "even pages" for the book |
| Never delete on assumptions | Nothing that does not repeat; text that runs on as a paragraph out of the strip is body text even where its first lines repeat; a sentence holding the page's number is not a page number; one page: nothing found (the window says why and offers drawing an area). Found while testing: the first version took repeated first body lines for header text and "Body text of page 1" for a page number - both fixed by these rules | "body text that starts in the top strip is not taken"; "one page: nothing found" |
| Odd / even layouts | Repetition is also counted over odd and over even pages; page-number groups are found per place | Book sample |
| Scanned pages via OCR | A page that is one picture with only invisible (recognised) text: its text lines are units, and long dark rules in the strips are found in a render of the page. Removal whitens those areas in the picture's own pixels and removes the recognised words; pages without recognised text are named, with the advice to recognise them first | "scans: ..." (header, rule and page number gone, picture white there, body text and its recognised words kept) |
| Highlight before deletion; review and exclude | The page with every item outlined: red and tinted when it goes, grey dashes when kept; untick in the list or click it on the page | Screenshots |
| Manual rectangular selection with resize handles | Draw an area; eight handles and the inside move or resize it; it applies to every chosen page (kept at the same distance from the foot when drawn in the lower half); Delete key or "Remove drawn area" removes it | Screenshot: area with handles |
| Pages: current, all, specific, range, combined, odd, even; All by default, stated | "Pages" list with All pages first; "These pages:" takes 2,5,8 / 3-15 / 1-3, 7, 9-12; the summary names the pages ("on all 6 pages", "on 2 pages") | "pages 2,4-5 only", "header only, even pages"; in the app pages 2,4 |
| Header only, Footer only, Both, Page numbers only, Selected only | "What to delete" list; ticking by hand switches it to "The items ticked below" | Smoke tests per mode |
| Before / after preview, navigation, counts, exclusions, confirmation | Before / After toggle (the after view is a copy of the page with the plan carried out), page arrows and Page Up / Down, a summary with items, pages and objects, a confirmation that states the removal is permanent and undoable | In the app: confirmation, Yes, saved, Undo, saved again (text gone, then back) |
| Permanent removal, not searchable | Objects are taken out of the page and the content written again; nothing is drawn over | "removed, not covered: the text is gone" |
| Shared XObjects | A form wholly in a strip (a letterhead used by every page) is removed from the chosen pages only (the form stays for the others). A piece inside a form that also draws the body (a page template) is listed as "part of a page template, kept": this PDFium cannot write such a form again after a piece is taken out of it, and a first try left the template without its font, so it is never edited | "shared letterhead removed on the chosen pages only", "page template kept whole" |
| Warn when a region overlaps body content | Pieces of the page's own content inside a drawn area are counted and quoted ("Attention: the area you drew also takes in 6 pieces... for example ..."), pieces across its edge are counted and kept; the confirmation repeats the warning and then defaults to No | "a drawn area over body text is reported"; screenshot |
| Undo / redo, Save / Save As; bookmarks, links, annotations intact | One undo step; only page content objects are touched | "body text, links and bookmarks kept"; in the app Undo and Redo |
| In the editing tools, no duplicates | Edit PDF menu, beside Watermark and Page numbers: "Add headers and footers..." (renamed from "Headers and footers...", moved out of Edit pages) and "Delete header and footer..."; the command palette has both | Menu check test passes; both chosen from the menu in the app (see B-046) |

## Installer (requested points)
The Inno Setup wizard was replaced by a native setup program (`installer/`), because Inno Setup 6.4 cannot draw its
buttons, check boxes and progress bar in a dark theme. Checked under Wine (which runs as administrator; Windows' UAC
prompt, the Windows 11 Default apps deep link and the dark title bar are Windows features not present in Wine).

| Request | Done | Checked |
|---|---|---|
| Screen 1: default `C:\Program Files\Feather PDF` on the system drive, read-only, Change button | Default from Windows' Program Files folder; shown in a read-only box; Change... opens the folder picker (a folder of its own, "Feather PDF", is added inside the one chosen). For updates and repairs the location is the installed one and Change is disabled | Screenshot; `/D=` and the picker path |
| Administrator rights | The setup's manifest asks for them (requireAdministrator) | Manifest in the exe |
| "Developed by Akshaya Simha", "Build Date: [build metadata]" | Both on screen 1; the date is read by `pack.py` from FeatherPDF.exe's own version information ("Built 2026-10-30") and also stored in the Installed apps entry | "Build Date: 30 October 2026" |
| Prominent Install button | Large accent button; says Update or Repair when Feather PDF is installed | Screenshots of Install, Update |
| Screen 2: real progress 0-100 % and the current activity | Driven by the bytes unpacked (1 MB blocks) and the steps after; the file being copied or the step is named | Screenshots at 0 %, during, 100 % |
| Graceful errors; permissions, disk space | Free space shown on screen 1 (in red, with Install disabled, when short) and checked again before copying; the folder is tested for writing first; a write error, a full disk or a damaged setup file ends on screen 2 with the reason, Try again (back to screen 1) and Close; files are copied beside the old ones and swapped in only when all are there, so a failure or Cancel leaves the old installation untouched and removes a folder made for the attempt | 5 MB drive: "Not enough free space here..." and Install disabled, silent run exit 1 with nothing left behind; `C:\blocked\Feather PDF` (a file in the way): "could not be created... Choose another place"; Feather PDF running: refused (exit 3 when silent, a Retry / Cancel question otherwise) |
| Success message and the two choices | "Feather PDF has been installed successfully!"; "Set Feather PDF as the default PDF application" (unchecked) opens Windows' own Default apps page (at Feather PDF on Windows 11) - Windows does not let programs set it themselves, and the setup does not try; "Launch Feather PDF" (checked) starts it through Explorer as the signed-in user, not as administrator; Finish | Finish with Launch: Feather PDF opened |
| Dark theme, branding, icon | Dark window drawn by the setup itself (buttons, check boxes, bar), the program icon and name, dark title bar where Windows supports it | Screenshots |
| Install, repair, update, uninstall | Same version again: repair (a deleted pdfium.dll came back); older version installed: update in place (the exe replaced, a file the old version had and the new one has not removed); `uninstall.exe` (Installed apps, or `/uninstall`): removes the files it installed, the shortcuts and every registry entry it made, the folder when empty; settings stay | Scripted test matrix: all checks pass |
| Start Menu shortcuts, Installed Apps entry | "Feather PDF" and "Feather PDF User Guide" for all users; Installed apps: name, version, publisher, icon, size, date, location, uninstall | Registry and Start menu folder checked |
| No duplicate installations | One entry and one folder: an installed copy keeps its folder (`/D=` elsewhere is ignored); a copy made by the earlier Inno Setup installer (per user or for all users) is uninstalled first, and what its uninstaller leaves is cleaned up | Inno copy in AppData: removed with its folder, one entry left |
| Windows 10 / 11 | Manifest declares Windows 10/11, per-monitor DPI; sizes scale with the display | - |
| No extra screens | Two screens for installing (and two for uninstalling) | - |

Found and fixed while testing: a quoted `/D="C:\Tools\My PDF"` kept a trailing quote (arguments are now parsed as
Windows does); the earlier installer's folder could stay behind, empty, because its uninstaller still held it (now
retried, else removed at the next restart); a failed attempt left an empty folder; the progress bar stood at 0 %
while the earlier installation was being removed.

## UI rework (requested points)
Each point of the request, what was done, and how it was checked (Wine, 1280 x 800 screen; for these checks Wine was set
to draw window frames itself, `HKCU\Software\Wine\X11 Driver\Decorated=N`, so the window looks as it does on Windows).

| # | Request | Done | Checked |
|---|---|---|---|
| - | Tab close button: red circle, white cross | Smooth red badge (drawn 4x and scaled down), brighter when pointed at; narrow tabs show it on the active and pointed-at tab | Zoomed screenshots, light and dark; click closes, tooltip "Close this tab" |
| 1, 24 | Less space at the top | No Windows caption: the tab row is the title bar (icon, tabs, +, minimise / maximise / close); 40 + 48 px instead of about 30 + 34 + 40 | Drag on the empty row moves the window (0,0 to 60,62); double-click maximises and restores; top edge resizes (y 0 to 58) |
| 2 | Compact tabs, + button | Tabs fit their name (120 to 240 px), shrink evenly when many are open (to 36 px), never run under the window buttons; + after the last tab | 2, 12 and 26 tabs |
| 3, 22 | Bigger, consistent icons | 20 px icons, one stroke weight on one grid, scaled with the display | Light, dark, 96 and 144 dpi |
| 4 | Brighter labels | Labels in the main text colour, not the dimmed one | Dark screenshot |
| 5, 6, 16, 18 | Spacing, groups, separators, borders | File, Undo / Redo, tools, then page, zoom, find / more at the right, with thin separators; lines under the tab row and the toolbar and over the status bar | Screenshots |
| 7 | Active tool highlighted | The menu button of the tool in use is tinted (also Side by side while on) | Rectangle: Annotate tinted; Edit paragraph: Edit PDF tinted; status bar names the tool |
| 8 | Prominent, editable zoom | A zoom box: type 200 and Enter; its arrow opens the presets and fit options | 200% applied; menu opens below the box |
| 9 | Navigation arrows with context | Previous / next beside the page box and "of n"; the page box takes a number and Enter; tooltips | Page 3 typed: page 3; 99 typed: "There is no page 99: this document has 5 pages." |
| 10, 11 | Search placement, bigger … | Find and … together at the right, 36 and 42 px wide, 10 px from the edge | Screenshot |
| 12 to 16 | Empty workspace, welcome text, Open PDF, drop area, recent files | Start page: title, Open PDF button inside a drop area, hint line, recent files (click opens, right-click removes, Clear list); on short windows the drop area shrinks so recent files stay visible | Light, dark, 144 dpi; opening from the list |
| 17, 25 | Separation, contrast | New palettes: dark title row, lighter toolbar, darker page surround; light theme likewise | Screenshots |
| 19 | Larger text | 10.5 pt interface font (was 10), 9 pt status bar, larger start page text | Screenshots |
| 20 | Status bar | Page n of m, page size with its name (A4, Letter...), the tool in use, Side by side, saved state | Typing a page, choosing a tool, an edit (Unsaved changes) |
| 21 | Scaling on large displays | Every size is designed at 96 dpi and scaled; fonts are remade when the window moves to a screen with other scaling; first window size and minimum size scale (minimum never larger than the screen) | Wine at 144 dpi |
| 23 | Readable disabled controls | Disabled grey about 4:1 against the toolbar (was near invisible in dark) | Screenshots with nothing open |

Also found and fixed while doing this: tooltips could sit over an opened menu (they are switched off while a menu is
open); the active tab's name was cut short (widths are measured in its bold font); the guide's pictures and window
description were redone for the new layout (`scripts/guide_overview.py` marks the overview).

## macOS version, stage 1 (requested points)
The request: a Mac version, in a repository with separate Windows and macOS folders. Decided with the user: a third
folder, `shared/`, for the engine both apps use; this stage is the foundation and a viewer. The plan for the rest is in
`macOS/PORTING.md`.

| Point | Done | Checked |
|---|---|---|
| Folders | `shared/` (engine), `Windows/` (Win32 app, resources, installer), `macOS/` (AppKit app); one CMake project builds the right one for the system | Windows: fresh configure and build, smoke tests ALL PASSED, installer rebuilt |
| Engine off Windows | `shared/platform.hpp` (the Windows types on the Mac), standard C file reading and safe saving, PDFium loaded from the app, form timers and pointer shapes from the app; passwords and signature checking say "not available on Mac yet" | 17 engine files build with Clang on Linux and Apple Clang on the Mac |
| Same results as Windows | `tests/engine`: 47 results (tables found and edited, headers and footers found and deleted) compared with the Windows app's | Linux: 47 the same. Mac: 46 the same and 1 a measurement 0.02 pt apart (PDFium on the Mac) |
| Mac app | Opens PDFs (Open, Open Recent, Finder, drag and drop), tabs, password, sharp pages on Retina, pinch and keyboard zoom, page navigation, night and dimmed page colours for every window, light and dark appearance, user guide under Help | GitHub Actions on a Mac: builds for Apple silicon and Intel, signs, engine check, pages drawn, app opens two PDFs as two tabs and draws its window (normal and night colours), disk image made |

Found and fixed on the way:
- **B-050** (Windows too): the letters of a ligature read in the wrong order. See above.
- **Mac, before release: document windows never appeared.** The window controller set its document in its own
  initializer. NSDocument's addWindowController: then took the window as already added and skipped it, so documents
  opened without a window. Found by the build check's trace ("window made", then "window controllers 0"). The document
  now attaches the window itself. Verified: the check reports one window per document and two tabs.
- **Mac: differences in the last digit.** Apple silicon fuses multiply-adds. The Mac build turns that off
  (`-ffp-contract=off`), so positions match the Windows app's.

## macOS version, stage 2: the features of the Windows app (first build of the whole batch)
The menus of the Windows app were brought to the Mac: Annotate and redaction, forms, page tools, print, presentation,
read aloud, palette, compare, editing (text, pictures, objects, tables), folder search, Settings and auto-update.
They were written while the repository could not be reached and built together for the first time on 30 October.
The build passes: Mac build, engine check, command-line check (export, compress, passwords, redaction, forms, compare,
text to PDF, hidden data, page tools, pictures, PDF/A, folder search, fonts, comment shapes) and the window check.
Built and checked is not the same as tried by hand. Each feature is still to be tried in the app on a Mac.

Found and fixed while building it:
- **Mac: the build was cancelled before it began.** GitHub is retiring its macOS 14 machines (2 November 2026) and
  switches them off for test periods. The build now runs on macOS 15.
- **Mac: compile errors** (a name used twice in the View menu; the toolbar's icon colour set on a type that has no
  such property; a method declared in the wrong place). Fixed. As a result, page up and down and zoom in and out
  (grouped buttons) do not take the colourful icon setting on the Mac yet.
- **Mac: started with files, the app stopped at "already open".** AppKit opens the files named at start, and the app
  opened them a second time; the new "already open" question then waited for an answer. The app now opens only the
  ones AppKit has not, and does not ask in the first seconds after start (the two opens can come in either order). Found by a sample of the stuck app in the build check. Verified: the check opens two PDFs as two
  tabs and finishes.
- **Mac: Properties and Fonts used: the lines ran over the heading and were cut off.** The sheet kept its first size,
  480 x 200, whatever it held. It now takes the size its contents need, and a note gets the height its text needs (the grid gave a note of
  several lines the height of one; the first fix alone did not cure it, as the next build's picture showed). Not verified
  yet: see the next build's pictures.
- **Check: folder search in subfolders** used the exam-paper sample, which needs LibreOffice and is not made on the
  Mac machine. It uses the picture sample now. Verified on Linux and to be seen on the Mac.

## Reported on the Mac, 31 October (screenshots)
- **B-051 (Windows too): editing a table drew the whole table again over the old one, in another font.** Seen in a
  syllabus PDF: after Apply, every cell showed its old text and a new copy in a sans-serif font, overlapping. Cause: the
  PDF keeps the whole page inside a form object (common in files saved from Word on a Mac). The editor found the table
  and its text, but none of the text objects were the page's own, so it could take nothing out and wrote every cell
  again on top. Fix (shared engine): before a table is edited, form objects on the page that hold text are opened up,
  their objects put on the page itself in the same place and order (the page looks the same). Reproduced with a test
  file built the same way (`tests/engine/cli_check.sh`, "tables inside a form object"); the check failed before the fix
  and passes after it on Linux. Windows smoke tests ALL PASSED. To be confirmed with the user's own file.
- **B-052 (Mac): a changed cell or line came out in another font.** A PDF often embeds only the letters it uses. When
  an edit needs a letter that is not there, the Windows app takes the full font from the computer; the Mac app had no
  such step and always used a standard PDF font (Times or Helvetica). The Mac now finds the installed font of the same
  name and style with CoreText and embeds it (TrueType files whose licence allows it; collections and CFF fonts still
  fall back). A font that is not installed on the Mac (for example Cambria, which only Microsoft Office brings, inside
  its own folder) still falls back to the closest standard font. Built; not yet tried on a Mac by hand.
- **Mac: Properties and Fonts used.** Properties: the lines ran over the heading (see stage 2 above; the fix was in
  builds that had not reached the user). Fonts used: the report area was empty. It is now the system's own scrolling
  text view. To be checked in the next build's pictures.
- **Mac and Windows: "could not find out whether there is a new version".** Nothing had been published to the releases
  repository yet, so the version file did not exist (the server answered 404) and the app blamed the internet
  connection. Now a 404 means "no newer version" on both, and 2.14.2 is to be published so the file exists.
- **Mac: ⌘⌫ deletes pages**, as in the Finder: Pages > Delete Selected Pages deletes the thumbnails selected, or the
  page shown (Undo brings them back). Not while typing in a text box (there ⌘⌫ deletes text).
- **Mac build check: OCR found no words on GitHub's Mac.** Those machines are virtual and Vision could not use a
  graphics processor. Recognition now tries again on the processor alone when Vision fails (also for anyone running
  macOS in a virtual machine).
- **Mac: Properties and Fonts used, again (the first two fixes did not cure it).** The build's pictures stayed the
  same after both fixes: the form's grid gave a note of several lines the height of one, whatever height the note was
  given. Now information boxes (Properties, Fonts used, Show Signatures) are the system's own alert sheet, which sizes
  itself (long reports in a scrolling text area), and in other forms notes sit in a plain column above the fields
  instead of in the grid. The build check now also pictures a form with a note and fields (Recognise Text).
- **Mac: selecting several thumbnails did not last.** Found in the build's picture of Select All Pages: only page 1
  stayed selected. The page view tells the sidebar which page is shown, and the sidebar then selected that page alone.
  Now a selection of several pages that holds the page shown is kept.
- **Mac: the fonts table in Fonts used was squeezed** into the narrow alert. Reports with tables go in the wide
  scrolling text area.
- **Published: Feather PDF 2.14.2 for Windows and macOS** in akshsim1991/FeatherPDF-releases, with `latest.json`. The
  file is served at the address both apps check, and both downloads match the SHA-256 in it (checked by downloading
  them again). The "could not find out whether there is a new version" message is therefore gone for both apps. An
  update from one version to the next can only be tried end to end when the next version (2.14.3) is published; until
  then Help > Update to New Version answers "You have the newest version".

## Reported on the Mac, 31 October: the app quit when OCR was started
- **B-053 (Mac): Feather PDF quit as soon as Recognise was pressed** (EXC_BAD_ACCESS on the main thread). The OCR form
  handed its answers to a block that ran a moment later; the block kept a reference to the answers, not a copy, and
  the form had freed them by then. The signing form had the same fault (it would have crashed after "Choose
  Certificate"). Both now copy the answers first; no other form does this (checked with a scan of every form's
  callback). Why the build did not catch it: its OCR check ran from the command line, which does not use the form. The
  build check now opens Recognise Text on a scanned page, presses Recognise as a user would, and requires words to be
  found and the app to finish.

## Known open issues on the Mac (31 October, updated for 2.14.4)
Each one is removed from this list only when it is fixed and checked.
1. ~~OCR crash in the published download~~ Fixed in 2.14.3 (published).
2. Updating from one version to the next: 2.14.3 and 2.14.4 are published; to be tried by the user (Help > Update to
   New Version).
3. ~~Fonts in collection files (.ttc: Helvetica, Times, Avenir...) could not be embedded~~ Fixed in 2.14.4: the one
   font needed is rebuilt as a font file of its own from its tables (checked with fontTools, at its strictest checksum
   setting, on a collection font on Linux and on Helvetica on the build Mac).
   ~~Fonts that only Microsoft Office brings (Cambria, Calibri...) were not found~~ Fixed in 2.14.4: Office's own font
   folders are made known to the app at start (nothing is installed on the Mac); not checked on a Mac with Office.
   Still open: fonts with PostScript (CFF) outlines, a few .otf fonts, cannot be embedded by PDFium; edited text in such
   a font uses the closest standard font. A font that is not on the Mac at all still falls back the same way.
4. The table fix (B-051): checked with a test file made the same way; to be confirmed with the user's PDF.
5. ~~Grouped toolbar buttons~~ Fixed in 2.14.3.
6. ~~Side by side was two windows~~ Fixed in 2.14.4: both documents in one window, left and right or top and bottom,
   linked scrolling, swap; the other document stays open in its tab. The build check opens two PDFs, chooses Side by
   Side, presses Show and requires two views in one window (seen in its picture).
7. ~~Notes in forms were always above the fields~~ Fixed in 2.14.4: notes stay where the form puts them, with the
   fields before and after them lined up.
8. ~~OCR languages by code~~ Fixed in 2.14.3.
9. and 10. Set aside by the user (Apple signing; signing with keychain certificates on a real Mac).
11. Features not yet tried by hand: only the user can do this; the build checks now also press buttons in forms
   (OCR, side by side) as a user would.
12. Not a bug: one measurement of the engine check differs from Windows by 0.02 pt because the PDFium library for the
   Mac computes the outer edge of a 1 pt line a hair differently. It changes nothing on screen or in files.

## Version 2.14.3 (both apps)
From now on every release has a new version number, so installed copies are offered it (Help > Update to New Version
and the check at start). 2.14.3 carries the OCR crash fix (B-053), grouped toolbar buttons in colour and OCR language
names on the Mac, and the same Windows app with its new number.

2.14.3 published in akshsim1991/FeatherPDF-releases on 30 October; both downloads match the SHA-256 in latest.json
(checked by downloading them again).

## Version 2.14.4 (both apps)
Mac: side by side in one window, forms keep their notes in place, fonts from collection files and from Microsoft
Office kept when text is edited. Windows: the same app with its new number.
2.14.4 published in akshsim1991/FeatherPDF-releases; both downloads match the size and SHA-256 in latest.json (checked by downloading them again).

## Reported on the Mac, 31 October: editing a line or a paragraph
Seen in the user's screenshots (a question bank made in Word; numbered, justified questions):
- **B-054 (both apps): a numbered item's paragraph took the end of the item before it and left out its own last line**
  (question 10 lost "diagram."). The list number ("10.") was read as a line of its own between the item's lines, so the
  paragraph stopped after one line and joined the line above. Numbers and bullets are now recognised: each starts an
  item, and is left as it is. Reproduced with a LibreOffice file and a reportlab file built the same way; checked by
  `cli_check.sh` ("numbered list items"), which failed before and passes now on Linux and on the build Mac.
- **B-055 (both apps): hyphens could become spaces** ("proportional-plus-integral"). Hyphen look-alikes (non-breaking
  and soft hyphens) are now read as hyphens, and a text object whose text cannot be read from it directly is read from
  the page's text. The test files keep their hyphens; the user's own file is still to be tried.
- **B-056 (both apps): a justified paragraph of two lines read as "Left".** It is now justified when its first line
  reaches the right edge of its column (shared with other lines) and its last line is shorter.
- **B-057 (Mac): the editor showed the old text through it, in another font, in the wrong place, with its style bar
  over the paragraph above, spell-check marks, and no wrapping.** The editor was a plain box over the page. Now: the
  page's own text is hidden while it is open (put back on close; nothing in the file changes); the text sits on the
  page's baseline in the document's own font (when the Mac has it), size and colour (also in night colours), wrapped
  at the paragraph's width with its line spacing, indents and alignment; spelling, grammar, smart quotes, dashes and
  replacements are off; the style bar is at the foot of the window, starts with the document's own font and exact
  size, and restyles the text as it is changed; Cancel and Apply (⌘Return) are on it. A zoom while editing applies.
  Checked in the build's pictures (a line, a paragraph). "Unsaved changes" with nothing changed: opening the editor
  no longer changes the document (the earlier status came from an earlier edit).

## Version 2.14.5 (both apps)
Editing lines and paragraphs: B-054 to B-057.
2.14.5 published; both downloads match latest.json.

## Requested 5 November: two-sided printing and the tab menu (both apps)
- **Print on both sides of paper**, with Flip on long edge and Flip on short edge. Windows: the print window had a
  "Sides" list with these choices; it is now a box "Print on both sides of paper" with the two choices under it
  (greyed until it is ticked), remembered for the next print. Found on the way: for landscape sheets Windows was asked
  for the other edge (the two-sided setting was swapped for landscape); long edge is now always long edge. Checked on a
  virtual screen with Wine (the window, ticking the box, choosing the short edge). Mac: a panel "Both Sides" in the
  print window with the same three controls, set on the print job as macOS's two-sided setting (long edge "no
  tumble", short edge "tumble"). To be tried on a printer that prints on both sides.
- **Right-click on a tab: Open directory having this file** (the folder opens with the file selected), Copy the file's
  path, Close tab, Close other tabs (Mac also: Move Tab to New Window). Windows: checked on a virtual screen with Wine.
  Mac: macOS draws the tabs and its own tab menu cannot be added to, so a right-click on a tab shows this menu instead;
  the build check lists the menu's items. Also File > Open Directory Having This File (⌥⌘R) on the Mac.

## Version 2.14.6 (both apps)
Two-sided printing with flip on long or short edge; the tab menu with Open directory having this file.
2.14.6 published; both downloads match latest.json.

## Version 2.14.7 (both apps)
About: Courtesy: Vinod Kumar C S, Athith D, Mamatha H R (checked in the Windows About box on a virtual screen).
2.14.7 published; both downloads match latest.json.

## Version 2.15.0 "Falcon" (both apps): separate windows
Requested: two PDFs at the same time in windows of their own (side by side on one screen, or one on each screen).
From this version each minor version has a name of its own, shown with the number in About and in the status bar.
- **Open in new window** (right-click on a tab), **a tab dragged off the tab row** (it opens where the mouse lets go),
  and **New window** (Windows: Ctrl+N and the "..." menu; Mac: File > New Window, ⇧⌘N).
- **Settings > Tabs > Opening files: As tabs of one window / Each in its own window.** For File Explorer or Finder,
  Open and Open Recent alike.
- Windows: each window is a copy of Feather PDF of its own (its own tabs, toolbar, side by side). A tab moved to a new
  window takes its unsaved changes with it (handed over in a file the new window reads and deletes). Each window keeps
  its own recovery copies (session-<process number>); after a crash, the next start offers the documents of every
  window that did not close normally, in one window. Checked on a virtual screen with Wine: Open in new window (the
  tab moved; two windows, one document each), Ctrl+N (a third, empty window), all three ended by force and the app
  started again (it offered both documents and reopened them; the old folders were removed), and a tab dragged off the
  row (a new window near where it was let go). Found on the way: the new window was not placed beside the old one
  (it found itself instead of the other window); fixed.
- Mac: macOS window tabs; a window opened by New Window or the setting can still be joined to others (Window > Merge
  All Windows). The tab menu's "Move Tab to New Window" is now "Open in New Window", as on Windows.
- Not across windows: "this PDF is already open" sees only the tabs of its own window.
2.15.0 "Falcon" published; both downloads match latest.json.

## Version 2.16.0 "Mustang" (both apps): 13 features
The 13 requested features, the new About text and saved workspaces; plan and checks in docs/ROADMAP-2.16.md, the list in
docs/CHANGELOG.md. Every engine part has command-line checks in tests/engine/cli_check.sh (run on Linux and in the Mac
build); the Windows screens were tried on a virtual screen with Wine; the Mac screens are driven by the Mac build check.
Not tried with real hardware here: a pen with pressure (Windows and Mac), real speech voices (Wine has none; a stand-in
voice was used), and the Mac screens by a person.

Defects found and fixed on the way:

### B-058 The command palette listed "Open a file" before "Pen" for "pen"
- **Status:** Fixed (2.16.0)
- **Root cause:** matches were listed in menu order; an exact name had no precedence over a word containing it.
- **Fix:** exact name first, then names starting with the words, then words starting with them, then anywhere.
- **Verified:** typing "pen" lists Pen first (virtual screen).

### B-059 Repair: the "repaired" copy still made other readers report errors
- **Status:** Fixed (2.16.0)
- **Root cause:** PDFium's default save appended to the damaged bytes; also a trailer entry pointed at a lost object.
- **Fix:** a full rewrite (FPDF_NO_INCREMENTAL); a dangling /Info entry blanked without moving offsets.
- **Verified:** poppler reads both damaged samples' repaired copies without an error (cli_check).

### B-060 Form checks complained about answers the user had not touched
- **Status:** Fixed (2.16.0)
- **Root cause:** every field breaking its rule was reported after each answer, including values already in the file.
- **Fix:** only an answer that changed since typing began is checked and, if refused, put back.
- **Verified:** virtual screen: a new price accepted (total recalculated), a quantity of 900 refused and put back to 7,
  the file's old bad values left alone.

### B-061 Accessibility: links were described as "another place" although they had a web address
- **Status:** Fixed (2.16.0)
- **Root cause:** the key lookup found "/URI" as the value of "/S" (the action type) before the "/URI" key.
- **Fix:** the address is the string after a "/URI" key.
- **Verified:** cli_check: the link reads "Link to https://example.org/safety".

### B-062 Mac build errors and a stuck build check
- **Status:** Fixed (2.16.0)
- **Root cause:** the accessibility namespace was called `access` (the POSIX function's name); two new files used the
  page view without importing it; the repair message was a modal alert, which held up the scripted exit.
- **Fix:** namespace `a11y`; the imports; the message as a sheet on the repaired document.
- **Verified:** the Mac build check.

2.16.0 "Mustang" published; both downloads match latest.json.

## Version 2.17.0 "Bronco" (both apps): blackboard page colours

Requested 6 November: a completely black (AMOLED) page with white text, like a blackboard, in two kinds: inverted, and
with the original colours of pictures and coloured content.

### B-063 Blackboard, original colours: a white line at the right edge of a picture
- **Status:** Fixed (2.17.0)
- **Root cause:** the area kept as drawn reached one pixel past the picture, so a column of white paper was left.
- **Fix:** the kept area ends at the picture's own last pixel.
- **Verified:** engine pixel check of the picture's four edges (paper black on both sides of each edge); Windows app
  screenshot under Wine.

### B-064 Blackboard: a light grey page shadow on the black
- **Status:** Fixed (2.17.0)
- **Root cause:** the page shadow of the Windows viewer is light grey, made for a grey surround.
- **Fix:** a faint dark grey edge in the blackboard modes.
- **Verified:** Windows app screenshot under Wine.

2.17.0 "Bronco" published; both downloads match latest.json.

## Version 2.18.0 "Ranger" (both apps): Combine PDFs and Convert to PDF

Requested 11 November: combine the PDFs open in the app into one, and Convert to PDF in File Explorer's right-click menu.

### B-065 Messages shown before the window opened were cut off
- **Status:** Fixed (2.18.0)
- **Root cause:** the message box measured its text with Windows' default dialogue font while the app's font was not yet
  set, but drew it with the larger system font, so wrapped lines ran past the bottom.
- **Fix:** it draws with the font it measured with.
- **Verified:** Convert to PDF of a Word file and of a damaged picture under Wine: every line shows.

### B-066 Several files opened at once from File Explorer could each open a window
- **Status:** Fixed (2.18.0)
- **Root cause:** each launch looked for the app's window before the first launch had made one.
- **Fix:** one launch at a time looks for the window and makes its own; the next waits (at most 15 seconds) until the
  window is ready, then hands its file to it.
- **Verified:** five Convert to PDF launches started at the same moment under Wine: one window with five tabs.

### B-067 Update to new version did nothing with more than one window open (Windows)
- **Status:** Fixed (2.18.0)
- **Reported:** 11 November, "Unable to update to the recent version through Feather PDF UI".
- **Root cause:** since 2.15 each separate window is a program of its own. Update now closed only the window it was
  chosen in; the installer, started silently, found Feather PDF still running, logged "Feather PDF is running" and
  stopped (code 3) without a message, so the window simply closed and nothing changed. Reproduced under Wine: 2.16.0
  with two windows, the 2.17.0 installer as the app starts it: exit 3, still 2.16.0, nothing restarted.
- **Fix:** Update now closes every Feather PDF window. The installer, when started by an update, waits for them, then
  asks to close any still open (Retry closes them, Cancel keeps the old version); a failed install is reported with its
  reason, and Feather PDF is started again in every case.
- **Verified:** under Wine, 2.17.0 with two windows updated by the 2.18.0 installer started as 2.17.0 starts it: the
  question appeared, Retry closed the other window, 2.18.0 was installed and started. One window: installed and started
  without a question. Not yet tried: a real Windows PC with the download from the releases site.

### B-068 A Mac update that could not be put in place was silent; cancelling Quit did not cancel it
- **Status:** Fixed (2.18.0)
- **Root cause:** the script that replaces the app after it quits ended quietly when the disk image would not open or
  the copy failed, and started the old version; when Quit was cancelled it still replaced the app after ten minutes.
- **Fix:** it leaves a note that the app shows at its next start; when the app did not quit it changes nothing.
- **Verified:** the script with stand-ins for hdiutil and ditto: good update (new version, opened), broken image (old
  version, opened, note written), Quit cancelled (nothing changed, nothing opened). Not yet tried on a real Mac.

### B-069 "Could not find out whether there is a new version" with the internet working (Windows)
- **Status:** Fixed in 2.18.0; the cause on the reporter's PC is not yet known for certain.
- **Reported:** 11 November, screenshot of 2.17.0 "Bronco" on Windows: Help > Update to new version says the internet
  connection should be checked, while the internet works.
- **Root cause (likely, two candidates):** the version file was read from one address only, raw.githubusercontent.com,
  which some Indian providers block at times (browsers often still reach it through their own DNS); and WinHTTP was
  opened with its own proxy setting, not the one set in Windows, so a PC behind a proxy, VPN or filtering program could
  not reach it. The message gave no reason, so the two could not be told apart.
- **Fix:** the proxy set in Windows (automatic too); TLS 1.2 and 1.3 asked for explicitly; when the first address
  fails, the jsDelivr mirror and then GitHub's API (installer downloads too, still checked by SHA-256); the message
  lists what each server answered. The Mac does the same (it already used the Mac's proxy).
- **Verified:** under Wine, the check reads 2.17.0 from raw.githubusercontent.com; with that server treated as
  blocked, the next ones are tried and each failure is listed (here api.github.com answered 403: this cloud machine's
  shared address had used GitHub's 60 requests an hour; cdn.jsdelivr.net is not allowed from this machine). The API
  address was checked with curl to give the version file and the 2.17.0 installer (the same SHA-256). Not yet tried:
  the reporter's PC; a 2.17.0 PC cannot reach 2.18.0 by itself if its check fails, so 2.18.0 has to be downloaded
  once by hand there.

2.18.0 "Ranger" published; both downloads match latest.json.

## Version 2.19.0 "Endeavour" (both apps)

### B-070 A .jpg converted to a blank white page
- **Status:** Fixed (2.19.0)
- **Reported:** 11 November, with a 1,926 KB wallpaper (1672 x 941) named .jpg: Convert to PDF made a white page.
- **Root cause:** a picture was taken to be a JPEG by its name. A PNG named .jpg (its size points that way: 1.9 MB is
  much for a JPEG of that size) was put in the PDF as JPEG data, which PDFium accepted but no reader can decode.
- **Fix:** a JPEG is recognised by its first bytes (FF D8 FF); anything else goes in as pixels. For Convert to PDF,
  New PDF from pictures and Insert picture alike.
- **Verified:** under Wine, a PNG named .jpg: before, a white page; now the picture. Engine check: such a file is no
  longer stored as JPEG data. Ordinary, large (4000 x 3000 and 9000 x 6000), progressive, CMYK and grey JPEGs still
  convert. The reporter's own file was not available here (only its picture was shared).

### B-071 Photos marked as turned by the camera lay on their side
- **Status:** Fixed (2.19.0)
- **Root cause:** the EXIF orientation of JPEG photos was not read.
- **Fix:** read; the picture is turned (and saved as JPEG again at high quality) and the page sized to match.
- **Verified:** under Wine, orientations 6 and 8: a portrait page, the corner mark where it belongs.

### B-072 Convert to PDF was not offered for PowerPoint files
- **Status:** Done (2.19.0); it was not part of 2.18.0, which converted pictures and text only.
- **Fix:** Office files are made into PDFs by the office program on the PC, through its automation interface
  (PowerPoint SaveAs ppSaveAsPDF, Word and Excel ExportAsFixedFormat; WPS Office's equivalents), else LibreOffice
  (soffice --convert-to pdf, with a profile of its own). The installer offers the entry for 12 Office extensions.
- **Verified:** under Wine (no Office there): the automation calls checked on Windows' own FileSystemObject (method
  with arguments in order, property, collection count, unknown name refused); without an office program the message
  says so. Not yet tried: real Microsoft Office, WPS Office or LibreOffice on a Windows PC.

### B-073 The update check took about 20 seconds
- **Status:** Fixed (2.19.0)
- **Reported:** 11 November, 2.18.0: "You have the newest version (2.18.0)" (correct: 2.19.0 was not published yet)
  came after about 20 seconds.
- **Root cause:** the servers were tried one after another, and the first (raw.githubusercontent.com), which does not
  answer on the reporter's network, was given up to 15 seconds to connect before the next was asked. It also confirms
  why 2.17.0 could not check at all there.
- **Fix:** all servers are asked at the same moment; the first good answer from raw.githubusercontent.com or GitHub's
  API is taken, a jsDelivr answer once neither has answered within two seconds more; the download starts with the
  server that gave the version file. The menu shows "Looking for a new version…" at once. The Mac does the same.
- **Verified:** under Wine: the check takes 0.1 s; with the other two servers made 20 seconds late (a test switch),
  the answer still comes in under a second. Not tried here: an answer from jsDelivr or GitHub's API (this cloud
  machine cannot reach jsDelivr, and GitHub's API limit for its shared address was used up).

2.19.0 "Endeavour" published; both downloads match latest.json.

## Version 2.19.1 (Windows)

### B-074 Swiping left or right on a touchpad did not scroll the page
- **Status:** Fixed (2.19.1)
- **Reported:** 11 November, a laptop touchpad.
- **Root cause:** the page view handled only the up-and-down wheel (WM_MOUSEWHEEL); sideways scrolling arrives as
  WM_MOUSEHWHEEL, which nothing handled. Shift + wheel was not sideways either. The Mac was not affected (its scroll
  view does both).
- **Fix:** WM_MOUSEHWHEEL scrolls sideways (right is positive), Shift + wheel too, at the same speed as up and down.
- **Verified:** under Wine, zoomed in so the page is wider than the window: sideways scroll events (X buttons 6 and 7)
  moved the page right and back left; Shift + wheel moved it sideways. Not tried here: a real touchpad.

2.19.1 published; both downloads match latest.json.

## Version 2.19.2 (Windows)

### B-075 Feather PDF did not start on Windows 7: "entry point BCryptHash could not be located"
- **Status:** Fixed (2.19.2)
- **Reported:** 13 November, screenshot of Windows 7.
- **Root cause:** SHA-256 and the other hashes (signatures, passwords of AES-256 PDFs, checking a downloaded update)
  called BCryptHash, which Windows has only from Windows 10. Windows 7 refuses to start a program that imports a
  function it lacks. No other import is newer than Windows 7; the three newer functions used (GetDpiForWindow,
  GetPointerType, GetPointerPenInfo) are already looked up while running, with a way round when missing.
- **Fix:** BCryptCreateHash, BCryptHashData, BCryptFinishHash (Windows Vista and later), the same results. The
  manifests list Windows 7, 8 and 8.1 beside 10. tests/win7check.py, part of tests/smoke.sh, fails when the program,
  pdfium.dll or the setup imports a function of Windows 8 or later.
- **Verified:** the check finds BCryptHash in the 2.19.1 program and nothing in the new one, pdfium.dll or the setup;
  the Windows tests all pass with Wine set to Windows 7 (build 7601), signatures and encrypted PDFs included, and the
  window opens. Not tried here: a real Windows 7 PC (Wine has the newer functions too, so only the import check shows
  their absence).
- **Still to know on Windows 7:** its WinHTTP speaks TLS 1.2 only after update KB3140245; without it the update check
  cannot reach GitHub's servers and says why ("the secure connection failed").

2.19.2 published; both downloads match latest.json.

## Version 2.20.0 "Aspire" (both apps): Edit PDF in one click

Requested 13 November: editing as in modern PDF editors. One click on Edit PDF shows the outlines of what can be edited,
and a click in text edits it directly, without choosing "line" or "paragraph" first.

| What | How | Verified |
|---|---|---|
| The outlines | pageobj::editableBlocks: each paragraph, grouped by the same rule as the paragraph a click edits (one shared function), and each picture (also inside groups) | engine: all 15 paragraphs of the report sample outlined, each the same block a click in it edits; the picture of a sample outlined |
| Windows edit mode | Edit PDF switches it on and off; a frame round each block, the one under the pointer stronger and tinted; text cursor over text, move cursor over a picture; a click in text opens the in-place editor with the caret there; a click on a picture picks it with handles (move, resize); the frames step aside while typing | under Wine: one click showed the second row and every frame; the red line lit up under the pointer; a click opened the editor there; typing and Ctrl+Enter changed it (the lines below moved down); the picture was picked and dragged; Esc ended editing and the row went |
| The second row | Edit text and pictures, Add text, Insert picture, Insert table, Find and replace, Redact an area, More editing tools (the whole former Edit PDF menu), Done | under Wine (each button's command is the menu's own) |
| Mac edit mode | Edit PDF in the toolbar; a bar under it with the same tools and Done; the same outlines, hover, cursors, caret at the click and picture dragging | the Mac build check opens edit mode, finds the blocks and clicks into a paragraph: the editor opens with the caret inside the text |

### B-076 Lines in different colours were taken as one paragraph
- **Status:** Fixed (2.20.0)
- **Root cause:** a paragraph was lines of the same size, spacing and column; a red line under a black one joined it.
- **Fix:** a line in another text colour starts a paragraph of its own (for editing and for the outlines alike).
- **Verified:** engine check: black, black, red, black lines give three blocks, the two black lines together; the demo
  page's black, red and blue lines are separate blocks.

2.20.0 "Aspire" published; both downloads match latest.json. The releases page now also links each file from the jsDelivr mirror (at the release commit), for networks that block raw.githubusercontent.com.

## Version 2.20.1 (both apps)

### B-077 A LaTeX bold heading opened for editing as plain sans-serif, on two lines, over the next line
- **Status:** Fixed (2.20.1)
- **Reported:** 13 November, screenshot: "Module 5 (b)" (bold serif) in the editor as Sans-serif, not bold, broken
  after "Module 5", the box covering "1. Define a material handling system."
- **Root cause:** the style of a font was guessed from a few words in its name ("times", "serif", "bold"); LaTeX's fonts
  are called "NimbusRomNo9L-Medi" (bold), "NimbusRomNo9L-Regu", "CMBX12", "LMRoman12-Bold" ..., with flags that do not
  say serif. The heading was taken as regular sans, wider than its bold serif original, so it broke into two lines.
- **Fix:** one style guess for both places that used it: the subset prefix left out; TeX, Nimbus, TeX Gyre, Latin
  Modern, Office and common foundry names; the font's own weight (600 and more is bold) and italic angle.
- **Verified:** engine checks on a page whose fonts are named as pdfTeX names them (flags 4): the heading is a bold
  serif, the body a regular serif; under Wine the editor opens on one line with Serif and B on, and the saved file
  keeps the heading's own bold font (Times Bold for letters its subset lacks), every other line where it was.

### B-078 A one-line heading broke onto a second line as soon as it got longer
- **Status:** Fixed (2.20.1)
- **Root cause:** a paragraph wraps at its widest line; a single line has no room to grow.
- **Fix:** a single left-aligned line wraps at the right edge of its column (the farthest any line starting at the same
  left edge reaches on the page).
- **Verified:** engine check: "Module 5 (b)" + " - Revised" stays one line; under Wine the same in the app.

2.20.1 published for Windows (13 November, at the user's request, before the Mac build check finished): the installer matches latest.json; latest.json gives the Mac no new version until its 2.20.1 is built, so Macs on 2.20.0 are not offered the same disk image again.

2.20.1 published for macOS too (the Mac build check passed once its test stopped passing "-word" arguments, which the Mac app reads as settings): both downloads match latest.json.

## Version 2.20.2 (both apps)

### B-079 A paragraph starting with a bold label: edited all in bold, its first line drawn twice (Mac)
- **Status:** Fixed (2.20.2)
- **Reported:** 13 November, screenshot (Mac): "License and cost controls: Paid third-party licenses ..." in the editor
  all bold, the first line a jumble of the box's text and the page's own.
- **Root cause:** a line's objects were joined only to the right of the first one met, which is the one with the highest
  top; the regular rest of the line came first (its parentheses are taller), so the bold label was left out and made a
  paragraph with the lines below, while the rest of line 1 was a block of its own, not hidden while editing. The style was
  the first object's (bold), and setting the text again used that one style for every word.
- **Fix:** the objects of a line are joined on either side (gap to the nearest one; a list number set apart stays
  apart); the paragraph's style is that of most of its characters; each character remembers the object it came from, and
  while the style is unchanged every word is set in its own object's font (typed words: the font of the text before).
- **Verified:** engine checks on a ReportLab page like the reporter's: the paragraph found whole with the label, in the
  body style; after an edit the label is Helvetica-Bold and the rest Helvetica (rendered: justified as before).

### B-080 "Paragraphs can use letters ... of Western languages" for text with a dash
- **Status:** Fixed (2.20.2)
- **Reported:** 13 November, screenshot: editing "51. Copy and move — Implement as ..." refused.
- **Root cause:** text for the standard fonts was allowed up to character 255 only; WinAnsi, which these fonts use, also
  has — – ‘ ’ “ ” • … € ™ and a few more.
- **Fix:** those are allowed (for paragraphs, text boxes, stamps, watermarks alike).
- **Verified:** engine check: an item with "—" edited, " Tested — done." added.

### B-081 After one edit, the other justified paragraphs of the page fell apart
- **Status:** Fixed (2.20.2)
- **Root cause:** when the page is written again, PDFium writes a justified line word by word; joining by the line's
  whole extent then refused a word lying inside it.
- **Fix:** the gap is measured to the nearest object of the line.
- **Verified:** engine check: a second edit on the same page finds its paragraph whole (both lines).

2.20.2 published for both apps; both downloads match latest.json.

## Version 2.20.3: diagnosis of lost recent files, and an audit of data safety

Requested 13 November: recent files disappear after updates or reinstallation; find the root cause, and audit the whole
application.

### B-082 Recent files (and reading positions, preferences) lost when several windows closed - Critical (Windows)
- **Status:** Fixed (2.20.3)
- **Root cause:** since 2.15 every window is a program of its own. Each read settings.ini once, at its start, and on
  closing wrote the whole recent list, all reading positions and every preference back from that copy. The last window
  to close won: what the others had added was gone. An update closes every window, so it happened at each update. The
  recent list was also written only when a window closed.
- **Reproduced:** under Wine, gamma.pdf in one window, alpha.pdf in another; the alpha window closed first, then the
  gamma window: the list held gamma, not alpha.
- **Fix:** the shared lists are read from the file, changed and written back at once, under a named lock (one window at a
  time); the recent list is written when a file is opened, removed or cleared, never from a window's copy; reading
  positions are merged (only the files this window read are written); a window writes only the preferences it changed;
  menus and the start page read the list as every window left it.
- **Verified:** tests/persistence.sh (new): two windows closing in either order keep both files; another window's theme,
  recent file and position survive a window closing.

### B-083 Names in other scripts saved as "?????" - Critical (Windows)
- **Status:** Fixed (2.20.3)
- **Root cause:** settings.ini, workspace files and the recovery list were 8-bit files (Windows writes a profile file in
  the code page unless it starts with a UTF-16 byte order mark); a Kannada file name became "????? ???????.pdf".
- **Reproduced:** under Wine, "ಕನ್ನಡ ಪ್ರಶ್ನೆ.pdf" opened: the recent entry and its position read "????? ???????.pdf".
- **Fix:** these files are UTF-16; an existing settings.ini is converted once (decoded in the code page it was written in,
  written beside it and swapped in, so an interruption leaves the old file); entries already damaged (a Windows path
  cannot hold "?") are left out of the lists.
- **Verified:** tests/persistence.sh: the old file converted with its theme and entries; the Kannada name kept; a new
  settings file starts as UTF-16.

### B-084 Nothing saved when Windows shut down, restarted or signed out - High (Windows)
- **Status:** Fixed (2.20.3)
- **Root cause:** settings were saved on WM_CLOSE / WM_DESTROY only; at the end of a session Windows sends
  WM_QUERYENDSESSION and WM_ENDSESSION and then ends the program.
- **Reproduced:** 2.19.2 under Wine: a file opened and scrolled, then the session ended (wineboot --end-session): neither
  its recent entry nor its position was saved.
- **Fix:** WM_ENDSESSION keeps the reading positions, the settings and the recovery copies of unsaved documents; closing
  a tab writes its position at once.
- **Verified:** tests/persistence.sh: after the session ends, the file's position (scrolled) is in the settings.

### Checked and found sound (no change)
- **Install, update, uninstall, reinstall (Windows):** the setup and the uninstaller never touch %APPDATA%\FeatherPDF;
  under Wine the settings file was byte for byte the same after an update, an uninstall and a reinstall, and the six
  recent files were listed again. The program folder is removed completely by the uninstaller (it finishes in a copy of
  itself a moment after returning).
- **Mac:** the recent list is the system's (NSDocumentController, kept by bundle identifier), settings are the app's
  user defaults; the build check now replaces the app as the updater does and requires both to be kept.
- **Saving:** a document is written to a temporary file, flushed, then put in place in one step, so an interrupted save
  leaves the old file; a full disk and a read-only folder are reported, the changes kept and Save As offered (tried in the
  app with a read-only folder). Documents are read whole into memory, so saving over the open file is safe.
- **Damaged input:** an empty file, random bytes, a cut-off PDF, text named .pdf and a missing file through export, block
  finding, fonts, repair and sanitize: errors, no crash or hang.
- **Long use:** a 1000-page PDF opened, paged through and closed six times in one window: memory 67 MB after each cycle,
  no growth.

- **Published:** 2.20.3 on 13 November (Windows installer and Mac disk image; both downloads checked against latest.json: size and SHA-256).

## Version 2.20.4: editing a page printed from a browser

Reported 15 November (screenshots): editing a bullet on a slide gave "there is no room on this page to move the text
below it down", then "The paragraph could not be changed", and the page's background turned black.

### B-085 Gradients turned black, symbols (Type3 text) disappeared, soft shadows turned solid, when a page was written - Critical (both)
- **Status:** Fixed (2.20.4)
- **Root cause:** every change to a page's objects is written back by PDFium (FPDFPage_GenerateContent), which writes each
  object from what it understands of it. It cannot write a fill with a pattern (a browser's gradient): the fill becomes
  black; nor text in a Type3 font (the browser's symbols and emoji): it is left out; and some graphics states (a soft
  shadow) lose their mask. In memory the page still looked right, so the damage showed only when the page was read again -
  after a change failed and the document was put back, or after undo, saving or reopening.
- **Reproduced:** a slide printed by Chromium (tests/engine/fixtures/printed-slide.html): adding one letter to a bullet
  made the background black (243,230,212 -> 0,0,0) and removed both arrows; the card's shadow became a grey block.
- **Fix (contentgen::generate, used by every feature that changes a page):** the page is drawn as it is held in memory,
  written, copied into a scratch document, read back and drawn again; the drawings are compared (softened by a 3x3
  average, a pixel counting only when nothing within one pixel of the other drawing is within 28 levels - a missing full
  stop still counts, a shape a fraction of a pixel apart does not). Where they differ, the objects PDFium cannot write are
  found by writing halves of them alone until each is alone, and each becomes a picture of itself at 300 dots per inch
  (transparency kept), at its depth, marked FeatherDrawing so that edit mode, the accessibility check and scan detection
  do not take it for a picture. If the written page still differs, the edit is undone with a message.
- **Verified:** tests/engine/cli_check.sh (new fixture): background kept, the rest of the page the same, no new pictures in
  edit mode; the Windows build under Wine: edit made in the app, saved, background and arrows in the saved file. 25 page
  writes in the other tests: all matched at once (nothing made a picture).

### B-086 A longer paragraph refused when anything near the bottom of the page was in its column - High (both)
- **Status:** Fixed (2.20.4)
- **Root cause:** everything below the paragraph in its column moved down as one block, so a footer 8 points above the
  page's edge made any longer paragraph "not fit", however much empty space there was.
- **Reproduced:** the slide above, and a page with a footer at 8 pt: refused.
- **Fix:** only what the longer text comes near moves, then what each moved object comes near, as a word processor moves
  the text below; refused only when something that has to move would leave the page.
- **Verified:** the bullets below moved down by the new lines, the footer stayed; tests/smoke.sh still refuses when the
  page is truly full.

### B-087 A one-line list item that wrapped continued under its bullet - Low (both)
- **Status:** Fixed (2.20.4)
- **Fix:** a one-line paragraph starting with a bullet or a list number ("1.", "a)", "(i)") hangs its next lines under its
  text. Verified: the wrapped line starts within 0.5 pt of the text after the bullet.

- **Published:** 2.20.4 on 15 November (Windows installer and Mac disk image; both downloads checked against latest.json: size and SHA-256).

## Version 2.21.0 "Puma": the first Android version

- B-088 (found while building it, Android only, fixed before release): search matches scrolled to the page's corner (the
  engine gives a text match as its letters; the bridge passed its empty rectangle on) and text added with Add text could
  not be found or selected (the page's text was not read again). Fixed in the bridge; checked by the bridge check (a
  match's place) and on the emulator (the first match on screen).
- **Published:** 2.21.0 on 16 November (Windows installer, Mac disk image and Android APK; all three downloads checked
  against latest.json: size and SHA-256).

## Version 2.22.0 "Kuga"

- B-089 (publishing, found while checking an update): right after a release is pushed, GitHub's servers hand out the
  old latest.json for a few minutes, but the old installers were already deleted, so an update started then failed to
  download. scripts/publish-release.sh now keeps the previous release's files one release longer. Verified: with 2.21.0
  published and 2.22.0 arriving, 2.20.4 goes and 2.21.0 and 2.22.0 stay.
- **Published:** 2.22.0 on 17 November (Windows, Mac and Android; all three downloads checked against latest.json: size
  and SHA-256; the Windows app's own update check reads 2.22.0 once GitHub's servers have the new file).

## Version 2.22.1

- B-090 (Android, Android 15): the page went under the title bar and the tools under the gesture bar (the system draws
  apps under its bars). The app keeps its bars clear of them on every version; measured by the emulator check. Found
  by a user's screenshot.
- B-091 (Android): the title bar said "Document" instead of the file's name. Fixed; the emulator check looks for the name.
- B-092 (builds): an Android build could not commit its files when an earlier build had committed the same pictures and
  APKs (git cannot merge them); the newer build's files are kept now (both workflows).
- B-093 (Android downloads): the APK for all phones was 22.7 MB, over the 20 MB the download mirror serves; one APK for
  each kind of phone now (8.9 MB, 6.2 MB).
- **Published:** 2.22.1 on 21 November (Windows, Mac, Android 64-bit and 32-bit; downloads checked against
  latest.json). The Windows suites could not run on this machine that day (Wine itself failed to start); the Windows
  app is 2.22.0 with a new version number.

## Version 2.22.2: editing PDFs made by WPS Office and by Word through Microsoft Print to PDF

Reported 21 November by a user (Mamatha), with two files and screenshots: a list of names in a table (WPS Office), and
an internal assessment question paper (Word, printed with Microsoft Print to PDF). "In the edit mode you cannot delete
the image", "the text is going in the next line", "editing the table is a huge blunder", "when you click, the texts are
not at all formatted". Every case below was reproduced with the engine on her files before it was fixed, and checked
on them after.

### B-094 A typed space drawn as a box - High (all apps)
- **Root cause:** an embedded font may hold only the letters the document uses. Which ones it holds was read from the
  page's text, which includes the spaces PDFium infers between words; the WPS fonts have no space, so a space was set in
  the font and drawn as its empty glyph (a box; "SHAHID^@MAHMADRAFIQ" when read back).
- **Fix:** the font's letters are taken only from characters really drawn in it; words in a font without a space are set
  as separate pieces a space's width apart.

### B-095 A WPS list drawn as a table: not found, letters interleaved, one paragraph per column, "SHAH I D" - High (all apps)
- **Root cause:** a one-column table was not taken as a table; WPS draws each letter as its own object at a large size
  scaled down by its matrix, so sizes and gaps were read 20 times too large; ruled rows were not paragraph breaks.
- **Fix:** a ruled one-column list is a table; letter sizes include the matrix; a line drawn between two lines ends a
  paragraph; spaces between letters come from the text page.

### B-096 Delete did nothing on a picture picked in edit mode - High (Windows)
- **Root cause:** edit mode picks pictures (frame and handles), but Delete, the picture menu and the picture actions
  checked for the Move tool only.
- **Fix:** one rule for "a tool that picks objects" (Move and edit mode) used by all of them. Not run under Wine (Wine
  fails to start on this machine); the Windows build compiles.

### B-097 A heading wrapped after a few letters were added - High (all apps)
- **Root cause:** a line on its own could grow only as far as other lines starting at its left edge reach; a centred
  heading has none, so its width was its own.
- **Fix:** it grows into the free space to its right, up to the right edge of the page's content, not onto anything
  overlapping its letters. Verified: "DEPARTMENT OF MECHANICAL ENGINEERING asd" stays on one line.

### B-098 A longer paragraph tore apart the table below it - Critical (all apps)
- **Root cause:** only objects under the paragraph's own column moved down; a wider table had half its cells moved
  ("Course" stayed, "Name:" moved; rules broken).
- **Fix:** everything below is considered; a table moves whole (refused, with a reason, when the table starts beside
  the paragraph); objects on one line a few points apart move together (the footer's page number).

### B-099 Every font of a Microsoft Print to PDF file shown as Serif and bold - High (all apps)
- **Root cause:** that printer names fonts "CIDFont+F1" ... and gives them serif flags and a heavy stem width.
- **Fix:** for such placeholder names the font's own name is read from its embedded file's name table (Calibri,
  TimesNewRoman-Bold ...) and the style taken from it.

### B-100 Table text edited as page lines, across cells and rows - Critical (all apps)
- **Root cause:** edit mode grouped the page's lines, and a line ran across the cells of a row ("1 Define ... CO1 L2");
  a cell's lines were split at a superscript, and joined with the next row.
- **Fix:** in a table found on the page, each cell is one block: its outline, its whole text, wrapped at its lines, set
  again by the table engine (the row grows, the table moves what is below it).

### B-101 Changing a cell set other rows again, losing superscripts and alignment - High (all apps)
- **Root cause:** a cell whose text object it shares with another cell (Word prints a row in one object) was always set
  again.
- **Fix:** cells sharing objects are kept together when none of them changed.

### B-102 Justified cells read as centred, set again centred - High (all apps)
- **Root cause:** a cell's alignment came from the gaps beside its text; full lines have equal gaps, so "centred". The
  table engine had no justified text.
- **Fix:** justified cells are recognised (lines starting and ending at the same places, as far from both lines, the
  last shorter; superscripts left out) and set justified; the table editors offer Justified.

### B-103 Superscripts flattened in a changed cell - Medium (all apps)
- **Fix:** a raised, smaller 1, 2, 3, o or 0 becomes ¹ ² ³ ° (in every font); a superscript raised far enough to make a
  line of its own is joined to its line first.

- **Tests:** tests/engine/cli_check.sh has a question paper made like hers (fonts renamed CIDFont+F1 with serif and bold
  flags, a ruled table with justified cells and m³): no outline crosses a cell's line, a cell edits whole in its own font,
  stays justified with m³, the other row is not set again, a heading grows on its line, a wrapping heading moves the
  whole table. All suites pass (cli_check 164, engine matches Windows 47, Android bridge 50).

### B-104 A justified cell read as left-aligned on the Mac - Medium (all apps; seen in the Mac build)
- **Found:** the Mac build check failed: the question paper's cell, set in Apple's Arial, was read as left-aligned, so
  an edit lost its justification. On Linux, with Liberation Sans, it passed.
- **Cause:** a line's ends were measured by its letters' ink. Fonts with the same widths place a letter's ink
  differently inside its width (Arial and Liberation Sans do), so lines that end together where they are set ended a
  point or two apart by ink, beyond the tolerance.
- **Fix:** lines are measured where their letters are set (PDFium's typeset box: origin to advance), which is the same
  for every line of a justified paragraph in any font.
- **Tests:** cli_check.sh makes a copy of the font with every letter's ink moved by up to 12% of an em, the widths kept;
  the old engine read the cell as centred, the new one as justified. The same cell checked in Liberation Sans, DejaVu
  Sans, FreeSans, Carlito and Liberation Serif. All suites pass (cli_check 165, engine matches Windows, Android bridge).

**Published:** 2.22.2 for Windows and Android first (21 November, at the user's request, while the Mac build ran), then
the Mac version from the same commit (1a9f25e) a few minutes later. The three downloads match latest.json (size and
SHA-256); the 2.22.1 files stay one release longer.

## Version 2.23.0 "Explorer": an editing evaluation on real PDFs, and its findings fixed

An evaluation edited six real PDFs from six programs (Word via Microsoft Print to PDF, WPS, Google Docs, TCPDF, htmldoc
stamped by a journal site, pdfTeX) with 49 checks, each looked at before and after (report: claude.ai artifact
"Feather PDF Editing Evaluation"): 24 worked, 6 had flaws, 18 failed, 1 was not supported. The user then sent two more:
a Word assignment with equations, and slides saved as pictures with recognised text. All findings below are fixed and
kept as tests/engine/realworld.sh (public files downloaded from GitHub; the user's own files only where present).

### B-105 Edits refused on Google Docs and htmldoc PDFs - Critical (all apps)
- **Cause:** PDFium does not write text set in Type3 fonts (Google Docs' flag and emoji letters; it also leaves an open
  "q"), and the write-back check drew a copy of the page that lost resources only fonts refer to. Objects PDFium cannot
  write were made pictures, but the pictures were judged pixel by pixel and differed by smoothing; htmldoc's inline
  images likewise.
- **Fix:** the check is confirmed on the document as saved; areas of objects made pictures of themselves are judged by
  their look (mean difference and amount of ink).

### B-106 LaTeX paragraphs edited a line at a time; two-line titles split - High (all apps)
- **Cause:** a page's lines are listed across the page, so in two columns the line before one was the other column's.
- **Fix:** a line's neighbours are the nearest lines in its column. A line that stops short of its column (the next
  line's first word would have fitted) ends its paragraph; short lines that each end a sentence stay apart; centred lines
  at ordinary spacing join. TeX's line-end hyphen (character 2) joins its word.

### B-107 Inserting or deleting a row scrambled Word tables; the footer moved - High (all apps)
- **Cause:** Word prints a row's line across its cells as one object; it moved once per cell. Everything below the table
  moved by the full growth, footers included.
- **Fix:** each object moves once; what touches the table's bottom moves with it, text just under it keeps its distance,
  the rest moves only as far as it must.

### B-108 Find and replace broke layouts and missed words - High (all apps)
- **Cause:** it replaced inside each text object: no font for letters a subset lacked (bold serif in her paper), lines
  ran over their column and across table cells, words drawn a letter at a time were never found.
- **Fix:** each match's paragraph is set again as editing sets it; one undo step; a replaced word keeps its own style.

### B-109 Scanned pages could not be edited - High (all apps; offer to recognise: Windows)
- **Fix:** recognised (invisible) text over a picture is edited visibly, in the colour, weight and size of its words in
  the picture, over a patch blended from the picture around it; nothing below moves (a picture cannot). Recognised words
  are grouped into lines by their shared height, and OCR now gives a line's words one baseline and size. Windows: editing
  a scan without text offers to recognise it first.

### B-110 Equations and exponents broken by editing - High (all apps)
- **Cause:** Word's Cambria Math letters (U+1D400 on) came apart into two halves; exponents were blocks of their own;
  a fraction's numerator and denominator were one paragraph; Word's hyphen objects were left out of their line.
- **Fix:** mathematical letters read as plain letters, set in italic beside the equation font's digits; exponents and
  indices join their line as superscript and subscript characters and are set raised or lowered in any font; a fraction
  bar separates; objects on one baseline make one line.

### B-111 Smaller editing flaws found by the evaluation - Medium (all apps)
- Centred headings wrapped left-aligned (now centred, as wide as the free space, not onto a logo); a title wrapped though
  the page had room (it may reach the mirrored margin); a click between two lines found nothing (the nearest line); list
  numbers lost their tab (kept apart); a word in *italics* lost its style (the style of its first letter); Google Docs'
  Arial Italic read as bold (a font name's style part wins over its flags); apostrophes split from their words.

### B-112 Text inside a form object could not be edited; shapes could not be picked - Medium (all apps)
- **Fix:** a click on text inside a form object (a stamped or merged page) puts the form's contents on the page, colours
  as plain RGB (white boxes came out black); when that would turn text into pictures the page is left as it was. Drawn
  shapes are picked for moving and deleting.

- **Tests:** tests/engine/realworld.sh, 24 checks (15 on public files run in the Mac build); cli_check 165, engine matches
  Windows, Android bridge: all pass.

### B-113 "You have the newest version" though 2.23.0 was out - High (Windows and Mac)
- **Found:** the user's PC (India) kept saying 2.22.2 was the newest after 2.23.0 was published.
- **Cause:** raw.githubusercontent.com did not answer there and GitHub's API refused (60 requests an hour per address,
  shared by many people), so the only answer was jsDelivr's copy of the branch, which may be up to 12 hours old.
- **Fix (2.23.1):** a fourth source asks jsDelivr which commit the branch is at (data.jsdelivr.com) and reads the version
  file at that commit, which is never an old copy; it counts as a fresh answer. Copies older than 2.23.1 still read the old
  mirror copy until it refreshes, so 2.23.1 is installed once from the download page.

### B-114 A fraction's bottom wrapped onto two lines when edited; its bar stayed - High (all apps)
- **Found:** the user changed s(s + 2)(s + 4)(s + 6) to ...(s + 4d)... under a fraction bar: "6)" went to a second line.
- **Cause:** the line was taken as a centred line limited to the free width beside "G(s)H(s) =", and nothing tied it to
  the fraction's bar or its other side.
- **Fix:** a line in a math font is an equation's: edited on one line (the editor's box is as wide as the page allows).
  When a bar lies just above or below it, the bar is set to the longer of the line and the other side (keeping its left
  end and overhang) and both are centred over it; a standalone equation keeps its centre.
- **Tests:** realworld.sh: the user's edit, and the top and bottom edited together (26 checks).

### B-115 A paragraph's lines edited apart since 2.23.0 - Medium (all apps; found by the Android build check)
- **Found:** the Android emulator check: words added to a two-line paragraph went onto a line of their own.
- **Cause:** the rule that ends a paragraph at a line stopping short of its column took the column's edge from lines
  within 15% of the size, so 11 pt lines below a 12 pt paragraph made its first line look short.
- **Fix:** the column's edge is taken from lines of the same size (within 4%).
- **Also:** the Mac's 2.23.2 disk image built but was not committed (the branch had moved on); 2.23.3 is built again.

**Published:** 2.23.3 for Windows, Mac and Android (21 November), all three from commit 710cff8; Windows went out first
as 2.23.0 to 2.23.3 at the user's request. The three downloads match latest.json (size and SHA-256); the 2.22.2 Mac and
Android files stay one release longer.

### B-116 Equations could only be edited as text: no way to make s⁷ into s⁸, no superscript or subscript - High (all apps)
- **Found:** the user typed 8 over the 7 of s⁷ + 9s⁶ + ...: it became "s8⁷", a plain 8 beside the raised 7, and the text
  editor had no superscript or subscript button. A fraction's top and bottom were edited as separate lines of text.
- **Cause:** equations were edited as lines of text; a power was a character of the text, and only digits typed as ⁸
  became one. There was no notation for fractions or roots, and nothing that read an equation as a whole.
- **Fix:** an equation editor (shared/equation.cpp, used by all three apps). It reads the equation under the click from
  the page: the text in the mathematics font and what belongs to it (smaller text raised or lowered becomes a power or
  an index, text over and under a bar becomes a fraction, a radical sign with its line a root, the left side through the
  middle of the bar). It shows it in a one-line notation, with buttons and a live picture, and sets the result on the page
  with standard fonts (Times, Symbol) and drawn bars and radical signs, marked with its notation so it opens again exactly
  as typed. The text editors got x² and x₂ (Ctrl+Shift+= and Ctrl+= on Windows) and an Equation button.
- **Tests:** realworld.sh: the user's polynomial and fraction read whole, s^7 made s^8, the fraction's top and bottom
  changed, a fraction with a power in its bottom read; an inserted equation read back as typed, its power changed, wrong
  notation refused (34 checks, 3 new on public files). Android bridge: insert, read, change, preview, wrong notation,
  superscripts. Previews checked by eye: fractions, roots, sums with limits, integrals, limits, Greek, growing brackets.
- **Not yet:** the equation is set in Times and Symbol, not in the document's own Cambria Math (it is a font collection
  that cannot be embedded as it is); a matrix or a multi-line equation is not read as one.

**Published:** 2.24.0 for Windows (22 November). The Mac and Android 2.24.0 builds failed to compile (B-119); they go out
as 2.24.1.

### B-117 The app closed on clicking text in edit mode (PDFs whose pages are forms) - Critical (all apps)
- **Found:** the user's syllabus (pages put together by FPDI: each page one form, 94 forms sharing one resource list):
  Feather PDF closed as soon as editing started.
- **Cause:** a click on text inside a form opens the form onto the page (B-112). Here that failed (a stamp over the page
  and one dash could only be kept as pictures), and the failure put the document back by loading it again. The click's
  code (groupAt) went on using the page it had fetched before: freed memory. Reproduced on Linux with AddressSanitizer
  (SEGV in FPDFPage_GetObject from groupAt).
- **Fix:** opening a form is first tried on a copy of the document, so a refusal never reloads the open one; a refused page
  is not tried again until the document changes; groupAt fetches the page again after finding the paragraph. Only text
  taken out of the form counts when checking that nothing became a picture, and a character or two (≤ 3, ≤ 1%) is let
  through.
- **Tests:** realworld.sh, private sy.pdf: the click sequence of edit mode (equation, picking, paragraph) on page 1, page 4
  with the dash; 25 to 33 clicks per page edit without a crash under AddressSanitizer.

### B-118 A bulleted list in a table cell became one bold sans paragraph - High (all apps)
- **Found:** editing the "Assembly Drawing Exercises" cell of the syllabus.
- **Cause:** the cell's text was read with its lines joined unless a line ended well short of the edge; the editor then
  joined the rest too; the cell's style was taken from its first character, a bullet in the Symbol font; the bullets
  (➢, Wingdings) could not be set at all, so such cells were refused.
- **Fix:** a line that starts with a bullet or a number starts its own line; line breaks are kept in the editor; the style
  comes from the first letter; letters no font of the cell has are set in the standard Symbol or ZapfDingbats font; each
  item keeps its indent and the place its words start after the bullet, and a line set against the right edge stays.
- **Tests:** realworld.sh (sy.pdf page 2); cli_check 165 and the engine's golden files unchanged.

### B-119 The Mac and Android builds of 2.24.0 did not compile - High (Mac, Android)
- **Cause:** Mac: a variable captured by a C++ lambda was declared `__block`. Android: inside the buttons' `apply` blocks,
  `text = ...` named the surrounding function's `text` value instead of the button's.
- **Fix:** both corrected; the release check now waits for both builds before calling a version published.

**Published:** 2.24.1 for Windows (22 November). The Mac 2.24.1 built; the Android one did not compile (B-120): both
go out as 2.24.2.

### B-120 The Android 2.24.1 build did not compile - High (Android)
- **Cause:** the `text = ...` → `setText(...)` change of B-119 dropped the closing bracket of the equation editor's hint.
- **Fix:** the bracket; every changed Kotlin line read again (there is no Android compiler in the build container; the CI
  build is the check).

### B-121 Editing formulas one after another in a LaTeX table: overlaps and mixed formulas - High (all apps)
- **Found:** editing all 170 formulas of the user's fluid mechanics compendium (LaTeX, booktabs tables, Computer Modern).
- **Causes:** (1) a formula that became wider was set over the next one on its line; (2) the new formula's pieces were
  read as part of the next formula; (3) the table finder saw a table that is not there and gave a column edge in the
  middle of the formulas; (4) the TeX extension font's code 32 is a tall bracket, but PDFium also reports trailing spaces
  as code 32; (5) Greek letters were set upright; ∮ and the degree sign had no glyph.
- **Fix:** what follows the formula on its line (and its fractions' parts) moves right by as much as the formula grew,
  up to the next column (where far more lines start than anywhere else, and nothing near runs across); the editor's own
  formulas never join another one; code 32 in CMEX is a space; Greek letters lean (a sheared matrix); ∮ is ∫ with a
  ring; TeX's ◦ in a power is °; characters outside the Times fonts' WinAnsi are refused with a message.
- **Tests:** the edit-every-formula run (scratch harness, not committed: the PDF is the user's): 170 formulas edited,
  0 refused; cli_check, real-world 37, engine golden files and the Android bridge all pass.

**Published:** 2.24.2 built for all three (22 November); it went out with 2.24.3 below.

### B-122 Editing text in a LaTeX formula table moved rows apart and turned formulas into plain text - High (all apps)
- **Found:** the user's fluid mechanics compendium, every third line of names, conditions and units edited after all its
  formulas (145 text edits on top of 165 formula edits).
- **Causes:** (1) the table finder read a grid from text lining up (no drawn columns) and the cell writer set those
  "cells" again; (2) a paragraph's wrap width was the page's (514 pt) beside a formula 120 pt away; (3) a growing
  paragraph pushed its column's next rows only; (4) lines joined across a label-to-formula gap (1.7 em, below the 2.5 em
  column rule), the bridge being TeX's accent in its roman font; (5) the editor's own formulas were read as text;
  (6) fraction parts of the next row moved with a growing formula.
- **Fix:** only tables with a drawn vertical line are edited as tables; lines wrap before what stands to their right
  within the paragraph's height; in a page of rows, text below moves only if run into; a gap over 1 em ends a line where
  many lines start (a column) or between text and mathematics (TeX's roman counts as mathematics when it has no word);
  editor-made formulas are skipped by paragraphs; fraction parts move only with their own bar. Formulas over two lines
  are flagged.
- **Tests:** cli_check, real-world 37, engine golden files, Android bridge: all pass; the compendium (private, scratch
  harness): 165 formulas edited, 3 two-line formulas flagged and left, 131 of 145 text edits made in sequence (the 14
  refused say why: no room at the page foot, or symbols the fonts lack); every text line edited alone: 412 of 432.
- **Still:** inline mathematics with a power and an index on one letter (T*₀) inside a sentence comes out muddled when
  the sentence is edited; a header row's two cells can join; a formula pushed past its column's edge overlaps the next
  column (there is no room for it); formulas wrapped over two lines are read in parts.


**Published:** 2.24.3 for Windows, Mac and Android (22 November), all three from commit 04ec68c, with the note that it is a
big update to install. The three downloads match latest.json (size and SHA-256); the 2.23.3 Mac and Android files and
the 2.24.1 Windows installer stay one release longer.

### B-123 Journal papers: no text could be edited, moved or resized - High (all apps)
- **Found:** the user's IEEE Access paper (pdfTeX, then iText 7.1.1), pages 1 and 2: "This page uses drawing that could
  not be written back exactly" on every edit, move and resize; the banner could not be picked.
- **Causes:** (1) the title and running head are in a Separation colour space (PANTONE 3015 C). PDFium's writer cannot
  write such a colour and leaves it out, so the text came back black; the check against the page held in memory then
  refused every change. (2) The banner is an image inside a form XObject and the icons are drawings inside forms; the
  picker looked only for images and paths on the page itself.
- **Fix:** objects that do not write back faithfully are first set again in the plain colour they show (read from
  PDFium), but only after a render of the object alone shows it in that one colour (blended with white at its edges), so
  patterns, shadings and soft masks are never flattened; only what still differs becomes a picture of itself. A form
  with no text in it, under the click, is picked whole: as a picture when it holds one, else as a shape.
- **Tests:** new cli_check cases on a generated page with a spot-coloured title, a banner image in a form and an icon in
  a form (edit, colour kept, move, resize); the paper's pages 1 and 2 (private, not committed): paragraph edits, moves and
  resizes of text, the banner and both figures all made.

### B-124 Resize handles all showed the move cursor - Medium (Windows, Mac)
- **Found:** the user: the four-way arrow on a resize handle is confusing; each handle should show its direction.
- **Fix:** Windows: corners show the diagonal size cursors, new side handles the left-right and up-down ones, the four-way
  arrow only inside the box (in edit mode and with the Select tool). Mac: four corner handles (proportional) instead of
  one, side handles, and drawn double-arrow cursors (macOS has no public diagonal one). Side handles stretch one way and
  are offered for pictures and shapes, not text.

### B-125 Remaining issues from the compendium report - Medium (all apps)
- **Header row joined:** the column rule only split a line when the next column's text was joined on the right; a line
  started from the second cell took the first in from the left. Now either side. Column starts also count lines starting
  within a point either side (italic letters' boxes start a little apart), so a column is found where its starts spread.
- **Formula past its column:** when what follows cannot move further, the formula is set smaller to fit, down to 75 %.
- **T*₀ in a sentence:** a superscript directly followed by a subscript (or the reverse) is now set stacked, at the same
  place; the raised star has a superscript form (U+20F0) and is set nearly full size, as an asterisk already sits high.
- **Still open:** formulas LaTeX wrapped over two lines of a cell are read in parts (flagged in the editor's title);
  formulas are set in Times, not Latin Modern, in TeX documents.

