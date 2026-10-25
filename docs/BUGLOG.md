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
  into Print, B-008), Read aloud and Sign with a certificate (Edit PDF menu), Select all / Copy page / Copy area (page
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

## Found, not yet corrected

### O-001 Clicking beside the text with Edit text / Edit paragraph shows a message box every time
- **Status:** Open (sweep). A hint in the status bar would not interrupt.

### O-002 Page Up, Page Down, Ctrl+Home do not scroll the page
- **Status:** Open (sweep). Seen on single-page documents in this test run; the keys reached the window but nothing moved.
