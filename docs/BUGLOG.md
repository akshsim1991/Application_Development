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
