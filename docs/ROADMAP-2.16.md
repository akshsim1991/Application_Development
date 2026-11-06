# Feather PDF 2.16.0 "Mustang": work plan and progress

The request of 5 November 2026: 13 features, the About text (development history, AI contributions, credits and
contact), and the version and changelog. This file is the checkpoint: work resumes from the first item not ticked.
An item is ticked only when it is built and checked (engine: `tests/engine/cli_check.sh` on Linux and the Mac build;
Windows: `tests/smoke.sh` and, for screens, a virtual screen with Wine; Mac: the build's checks and pictures).

Each feature: engine (shared/, with a command-line check) -> Windows screens -> Mac screens.

| # | Feature | Engine | Windows | Mac |
|---|---|---|---|---|
| 13 | Saved workspaces | — | [x] | [ ] |
| 1 | Find and redact sensitive information (email, phone, Aadhaar, PAN, bank account) | [ ] | [ ] | [ ] |
| 2 | Search: whole word, wildcards, in comments | [ ] | [ ] | [ ] |
| 3 | Split by bookmarks / size / every N pages; merge keeping bookmarks; interleave odd and even scans | [ ] | [ ] | [ ] |
| 9 | Text compare, word by word, with a report | [ ] | [ ] | [ ] |
| 5 | Attachments (view, add, extract, save) and layers (show / hide) | [ ] | [ ] | [ ] |
| 10 | Repair; Fast Web View (linearize) | [ ] | [ ] | [ ] |
| 6 | Undo history panel | [ ] | [ ] | [ ] |
| 8 | Accessibility checker and fixes | [ ] | [ ] | [ ] |
| 4 | Comment threads (replies), Accepted / Rejected, review summary | [ ] | [ ] | [ ] |
| 12 | Read aloud with word highlighting, playback controls | — | [ ] | [ ] |
| 7 | Forms: calculated fields, input validation | [ ] | [ ] | [ ] |
| 11 | Pen and tablet: pressure, palm rejection | — | [ ] | [ ] |
| — | About: development history, AI contributions, credits, contact | — | [x] | [ ] |
| — | Version 2.16.0 "Mustang" everywhere; changelog; release | — | [ ] | [ ] |

## Notes

- Workspaces (Windows): saved, listed, reopened (tabs, order, active tab, zoom) and deleted; checked on the virtual screen.
- About (Windows): long text in a scrolling box, the e-mail check box opens the mail program.
