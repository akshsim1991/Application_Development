# Feather PDF 2.16.0 "Mustang": work plan and progress

The request of 5 November 2026: 13 features, the About text (development history, AI contributions, credits and
contact), and the version and changelog. This file is the checkpoint: work resumes from the first item not ticked.
An item is ticked only when it is built and checked (engine: `tests/engine/cli_check.sh` on Linux and the Mac build;
Windows: `tests/smoke.sh` and, for screens, a virtual screen with Wine; Mac: the build's checks and pictures).

Each feature: engine (shared/, with a command-line check) -> Windows screens -> Mac screens.

| # | Feature | Engine | Windows | Mac |
|---|---|---|---|---|
| 13 | Saved workspaces | — | [x] | [ ] |
| 1 | Find and redact sensitive information (email, phone, Aadhaar, PAN, bank account) | [x] | [x] | [x] |
| 2 | Search: whole word, wildcards, in comments | [x] | [x] | [x] |
| 3 | Split by bookmarks / size / every N pages; merge keeping bookmarks; interleave odd and even scans | [x] | [x] | [x] |
| 9 | Text compare, word by word, with a report | [x] | [x] | [x] |
| 5 | Attachments (view, add, extract, save) and layers (show / hide) | [x] | [x] | [x] |
| 10 | Repair; Fast Web View (linearize) | [x] | [x] | [ ] |
| 6 | Undo history panel | [x] | [x] | [ ] |
| 8 | Accessibility checker and fixes | [x] | [x] | [ ] |
| 4 | Comment threads (replies), Accepted / Rejected, review summary | [x] | [x] | [ ] |
| 12 | Read aloud with word highlighting, playback controls | [x] | [x] | [ ] |
| 7 | Forms: calculated fields, input validation | [x] | [x] | [ ] |
| 11 | Pen and tablet: pressure, palm rejection | [x] | [x] | [ ] |
| — | About: development history, AI contributions, credits, contact | — | [x] | [ ] |
| — | Version 2.16.0 "Mustang" everywhere; changelog; release | — | [ ] | [ ] |

## Notes

- Workspaces (Windows): saved, listed, reopened (tabs, order, active tab, zoom) and deleted; checked on the virtual screen.
- About (Windows): long text in a scrolling box, the e-mail check box opens the mail program.
- 1 Sensitive information: 10 real-shaped values found, 8 look-alikes left alone, redaction verified (cli_check); Windows on the virtual screen; Mac in CI (both forms, 10 marked).
- 2 Search: whole word, wildcards, comments (cli_check, 9 checks); Windows find bar on the virtual screen; Mac in CI (7 hits, 4th in a comment).
- 3 Split/merge/interleave: cli_check (13 checks with bookmark trees read back by pikepdf); Windows interleave and merge on the virtual screen.
- 3 Mac: interleave, merge (11 pages, 4 bookmarks) and split at chapters (bookmarks read back) in CI.
- 9 Text compare: cli_check (3 changes; a line moved to another page is not one; marked copy and report checked); Windows on the virtual screen; Mac traces right in CI.
- 5 Attachments and layers: cli_check (list, save byte for byte, add, remove; layers listed, switched, and drawn as set); Windows on the virtual screen (layers switched, attachment opened in Notepad).
- 10 Repair: index moved or missing repaired (9 pages, poppler reads it without errors, bookmarks kept), an intact file reported as fine. Fast Web View: our own linearizer (hint tables, outline table); qpdf's check passes on 45 test files with the pages looking the same; Windows on the virtual screen (repair report; the saved copy passes qpdf).
- 6 Undo history: every step named (engine and app), with its time; back or forward several steps at once (cli_check). Windows: a History tab in the side panel, tried on the virtual screen (three changes, back to the start, redo shown).
- 8 Accessibility: 13 checks (title incl. placeholder titles, title shown, language, tags, text on every page, Unicode, alternate text, links, field tooltips, tab order, bookmarks, contrast and order for a person); fixes for six, written and read back by pikepdf (cli_check); Windows dialog and report on the virtual screen.
- 4 Comment threads: replies (/IRT, /RT /R) and review states (/State, /StateModel Review) as the PDF standard has them, read back by pikepdf (cli_check); Windows dialog on the virtual screen (reply added and listed).
- 12 Read aloud: the script maps each spoken word to its page (cli_check); Windows: SAPI word events tint the word, the view follows it, controls below the pages (back/forward a sentence, pause, slower/faster, stop) tried on the virtual screen with the stand-in voice (Wine has no voice; real SAPI not tried here).
- 7 Forms: calculations (sum, product, average, smallest, largest) and checks (number, range, date, e-mail, phone) written as Acrobat's scripts and read back by pikepdf (cli_check); Windows on the virtual screen: a total recalculated as a price was typed, a quantity out of range refused and put back.
- 11 Pen: pressure strokes (the stroke kept for other readers, its look as wide as pressed; widths measured from the page picture in cli_check). Windows: WM_POINTER pen pressure, touch ignored while the pen tool is on or the pen was just used; mouse drawing checked on the virtual screen (no pen there). Mac: tablet pressure, non-pen input ignored while a pen is near. A real pen not tried here.
- Command palette: exact and word-start matches first ("pen" found Open before Pen).
