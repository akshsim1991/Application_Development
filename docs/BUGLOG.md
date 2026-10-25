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
