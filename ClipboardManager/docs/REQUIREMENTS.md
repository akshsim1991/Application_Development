# Requirement matrix — Feather Clipboard Manager 2.1.0

One row for each of the 216 feature rows in *Windows Clipboard Manager Development Specification*. **Status is stated honestly:** this build was written and tested on Linux with a cross-compiler and Wine, never on a real Windows PC, so many rows are "implemented, not tested on Windows".

| Status | Meaning |
|---|---|
| DONE-T | implemented **and** checked by an automated test or measurement |
| DONE-U | implemented and compiled, but never exercised on real Windows (at most seen in Wine) |
| PARTIAL | part of the row is implemented; the note says what is missing |
| NOT DONE | not implemented |
| EXCLUDED | deliberately not built (your decision) |

Evidence letters: **U** unit tests (`tests/run.sh`, 107 checks) · **W** self-test under Wine (`--selftest`, 47 checks) · **V** screenshot under Wine · **B** benchmark (`--bench`) · **R** code review only.

## Totals

| Priority | rows | DONE-T | DONE-U | PARTIAL | NOT DONE | EXCLUDED |
|---|---|---|---|---|---|---|
| P0 | 69 | 21 | 31 | 17 | 0 | 0 |
| P1 | 89 | 26 | 36 | 12 | 13 | 2 |
| P2 | 58 | 7 | 11 | 9 | 31 | 0 |
| **all** | **216** | **54** | **78** | **38** | **44** | **2** |


## 1. Clipboard Capture and History Management

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| CAP-001 | P0 | Automatic clipboard monitoring | DONE-U | AddClipboardFormatListener + WM_CLIPBOARDUPDATE (ui.cpp). Capture/restore code exercised under Wine (W); the live listener was not. |
| CAP-002 | P0 | Unlimited history option | PARTIAL | No item-count cap by default (0 = unlimited). The required low-disk-space protection is NOT implemented; a full disk gives an error message. |
| CAP-003 | P0 | Text capture | DONE-T | W: Unicode text, mixed scripts, multi-line round trip through restart. |
| CAP-004 | P0 | Rich-text capture | DONE-U | HTML and RTF are stored beside the text and restored (W checks the stored flags). Reading HTML/RTF from a real application was not exercised. |
| CAP-005 | P0 | Image capture | DONE-T | W: PNG and bitmap (DIB) put on the clipboard and read back; pictures stored as PNG files. |
| CAP-006 | P0 | File and folder capture | DONE-U | File lists are stored as references (W). Reading CF_HDROP from Explorer was not exercised. Restores always as "copy", never "cut". |
| CAP-007 | P0 | One-click history paste | DONE-U | Enter / double-click puts the item on the clipboard and sends Ctrl+V to the remembered window (SendInput). Not testable here. |
| CAP-008 | P0 | Duplicate detection | DONE-T | W: identical copy is not stored twice (hash of content). |
| CAP-009 | P0 | Chronological history | DONE-T | W + V: newest first. |
| CAP-010 | P0 | Manual item deletion | DONE-T | W: delete moves to Recently deleted (7 days), undo, delete for good. |
| CAP-011 | P0 | Clear entire history | PARTIAL | "Delete all unpinned items" and "Empty recently deleted" exist; there is no single "clear absolutely everything including pinned" command. |
| CAP-012 | P0 | Persistent history | DONE-T | W: history restored after restart; journal tolerates a torn last line. |
| CAP-013 | P1 | Configurable history limits | DONE-T | Age, item count and disk space limits (Settings). W tests the item limit; age and size limits are untested. |
| CAP-014 | P1 | Intelligent duplicate handling | DONE-T | W: duplicate moves to the top (recency updated). |
| CAP-015 | P1 | Source application identification | DONE-U | Clipboard owner process name, best effort. Shown in the preview, searchable (app:). |
| CAP-016 | P2 | Clipboard format inspector | DONE-U | More > Tools > Clipboard format inspector (lists formats and sizes, interprets nothing). Not run. |

## 2. Search, Filtering, and Organization

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| SEARCH-001 | P0 | Instant search | DONE-T | B: 100,000 items, one word 25 ms median (Wine). 180 ms typing debounce. |
| SEARCH-002 | P0 | Case-insensitive search | DONE-T | W/U. |
| SEARCH-003 | P0 | Search highlighting | PARTIAL | The first match is selected in the preview pane; list rows are not highlighted. |
| SEARCH-004 | P0 | Pinned items | DONE-T | W: pinned items survive cleanup. Sensitive expiry overrides pinning only until you pin it (pinning rescues it). |
| SEARCH-005 | P0 | Favorites | PARTIAL | Pinned items and snippets serve as favourites; there is no separate favourites flag. |
| SEARCH-006 | P1 | Tags | PARTIAL | One group label per item (tag: syntax, Group menu). No multiple tags per item. |
| SEARCH-007 | P1 | Categories | PARTIAL | Automatic kinds (text, picture, files, link, email, phone, colour, path, JSON) plus manual groups. No "code" category. |
| SEARCH-008 | P1 | Date filters | DONE-T | U/B: date:today, date:yesterday, date:7d, date:2026-11-13. |
| SEARCH-009 | P1 | Application filters | DONE-T | W/B: app:chrome. |
| SEARCH-010 | P1 | Content-type filters | DONE-T | W/B: type: filter and the Filter menu. |
| SEARCH-011 | P1 | Multi-keyword search | DONE-T | W: every word must match. |
| SEARCH-012 | P1 | Exact phrase search | DONE-T | W/U: "exact phrase". |
| SEARCH-013 | P1 | Saved searches | NOT DONE | Saved searches are not implemented. |
| SEARCH-014 | P2 | Regular expression search | DONE-T | U/W/B: re:pattern (ECMAScript, 200-char pattern, first 400 chars of each item). Slow at scale: about 0.6 s at 100,000 items (B, Wine). |
| SEARCH-015 | P2 | Fuzzy search | DONE-T | U/W/B: ~word tolerates one wrong, missing or extra letter, first 300 characters. About 0.7 s at 100,000 items (B, Wine). |
| SEARCH-016 | P2 | Duplicate grouping | NOT DONE | Duplicates are merged at capture time; there is no grouping view. |
| SEARCH-017 | P2 | Smart collections | NOT DONE | No rule-based smart collections (groups are manual). |

## 3. Copy, Paste, and Productivity Operations

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| PASTE-001 | P0 | Paste selected item | DONE-U | See CAP-007. |
| PASTE-002 | P0 | Copy without pasting | DONE-T | W: ClipPutText/ClipPutItem round trips. Ctrl+C in the window and Shift/Enter settings are UI paths not exercised. |
| PASTE-003 | P0 | Paste as plain text | DONE-U | Shift+Enter, context menu and Ctrl+Alt+P put text without HTML/RTF. |
| PASTE-004 | P0 | Keyboard-first operation | DONE-U | Key handling in ui.cpp (HandleKey). Not driven by real key events here. |
| PASTE-005 | P1 | Paste multiple items | DONE-U | Selected items joined (new line by default). |
| PASTE-006 | P1 | Sequential paste | DONE-U | Paste queue advances on each Ctrl+Alt+N press; the app cannot confirm that the other program received it (documented). |
| PASTE-007 | P1 | Paste queue | DONE-U | Right-click > Add to the paste queue (Ctrl+Q); More > Clear the paste queue. |
| PASTE-008 | P1 | Text case conversion | DONE-T | U: upper, lower, Title, Sentence. |
| PASTE-009 | P1 | Trim whitespace | DONE-T | U. |
| PASTE-010 | P1 | Remove extra blank lines | DONE-T | U. |
| PASTE-011 | P1 | Find and replace | DONE-U | Core FindReplace is unit-tested (U); the dialog was not driven. |
| PASTE-012 | P1 | Text snippets | DONE-U | Snippets: new, edit, save text as snippet; kept in the Snippets filter, never auto-deleted. Shown in a screenshot (V). |
| PASTE-013 | P1 | Paste without line breaks | DONE-T | U: Remove line breaks. |
| PASTE-014 | P2 | Paste templates | DONE-U | Core ExpandTemplate is unit-tested ({date} {time} {datetime} {iso} {year} {weekday} {month}); the menu path was not driven. |
| PASTE-015 | P2 | Clipboard macros | PARTIAL | Pipelines are the macro mechanism. Recorded keystroke/paste-sequence macros are not implemented. |
| PASTE-016 | P2 | Multi-item separator rules | DONE-U | Paste joined with: new line, comma, space, tab, nothing, semicolon. |
| PASTE-017 | P2 | Text transformation pipelines | DONE-U | Settings > Text tools: "Name = step / step / find:a=>b". Core RunPipeline is unit-tested (U). |

## 4. User Interface and Usability

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| UI-001 | P0 | Compact popup window | DONE-U | Compact view toggle; popup near the caret/cursor. Layout seen under Wine (V). |
| UI-002 | P0 | System tray integration | DONE-U | Tray icon, menu, pause state icon. Not visible under the test display. |
| UI-003 | P0 | Global keyboard shortcut | DONE-U | RegisterHotKey, default Ctrl+Shift+V. |
| UI-004 | P0 | Clear item previews | DONE-T | V: list and preview pane. |
| UI-005 | P0 | Responsive interface | PARTIAL | Virtual (owner-data) list; B: 100,000 items. Popup latency and UI smoothness were not measured on Windows. |
| UI-006 | P0 | Dark and light themes | DONE-T | V: light and dark seen; follows Windows setting. |
| UI-007 | P0 | Context menus | DONE-U | Context menu built per selection. |
| UI-008 | P0 | Tooltips | DONE-U | Tooltips on toolbar buttons. |
| UI-009 | P1 | Resizable window | DONE-U | Resizable, draggable splitter; layout seen (V). |
| UI-010 | P1 | Adjustable preview length | DONE-U | Settings > Appearance > Length of each line. |
| UI-011 | P1 | List and grid layouts | NOT DONE | List layout only; no thumbnail grid. |
| UI-012 | P1 | Adjustable font size | DONE-U | Settings > Appearance > Text size (small/normal/large). |
| UI-013 | P1 | Always-on-top mode | DONE-U | More > Keep on top, and Settings. |
| UI-014 | P1 | Window position memory | DONE-U | Window placement saved in the registry (HKCU). |
| UI-015 | P1 | Multi-monitor positioning | DONE-U | Opens near the caret or cursor, clamped to that monitor. |
| UI-016 | P1 | Customizable toolbar | NOT DONE | The toolbar is fixed. |
| UI-017 | P2 | Detachable preview pane | NOT DONE | The preview pane is fixed (can be hidden by Compact view). |
| UI-018 | P2 | Command palette | NOT DONE | No command palette. |
| UI-019 | P2 | Workspace layouts | NOT DONE | Only Compact view is saved. |

## 5. Keyboard Shortcuts and Navigation

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| KEY-001 | P0 | Customizable activation shortcut | PARTIAL | Chosen from three shortcuts (Ctrl+Shift+V, Ctrl+Alt+V, Win+Shift+V) or none; arbitrary key capture is not implemented. |
| KEY-002 | P0 | Arrow-key navigation | DONE-U | Up/Down in the search box move the list. |
| KEY-003 | P0 | Enter-to-paste | DONE-U | Enter pastes; Shift+Enter plain. |
| KEY-004 | P0 | Escape-to-close | DONE-U | Esc clears the search, then hides. |
| KEY-005 | P1 | Numbered quick-paste | DONE-U | Ctrl+1..9 in the window; Ctrl+Alt+1..9 from any program. |
| KEY-006 | P1 | Favorite-item shortcuts | NOT DONE | No per-favourite shortcuts. |
| KEY-007 | P1 | Shortcut conflict detection | DONE-U | A shortcut that cannot be registered is reported at start with advice to choose another. |
| KEY-008 | P1 | Keyboard shortcut reference | DONE-U | More > Tools > Keyboard shortcuts. |
| KEY-009 | P2 | Context-sensitive shortcuts | NOT DONE | Not implemented. |
| KEY-010 | P2 | Custom command bindings | NOT DONE | Not implemented. |

## 6. Image and Screenshot Management

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| IMG-001 | P0 | Screenshot history | DONE-T | W: PNG/DIB from the clipboard stored as PNG. |
| IMG-002 | P0 | Image thumbnails | PARTIAL | Pictures show in the preview pane; the list shows text ("Picture, 480x300") not thumbnails. |
| IMG-003 | P0 | Original image preservation | DONE-U | A PNG offered by the source is stored byte for byte; a bitmap-only source is converted to PNG. |
| IMG-004 | P1 | Full-size image preview | DONE-U | Preview pane; double-click for actual size. |
| IMG-005 | P1 | Save image to file | DONE-U | Save as PNG, JPEG or BMP. |
| IMG-006 | P1 | Image dimensions | PARTIAL | Width, height and file size are shown; the format is not named. |
| IMG-007 | P1 | Image resizing | DONE-U | Picture editor: Resize (75/50/25/200%). |
| IMG-008 | P1 | Image compression | PARTIAL | JPEG save at quality 90; stored history pictures are not recompressed. |
| IMG-009 | P1 | Format conversion | DONE-U | Save as PNG/JPEG/BMP. No WebP/TIFF/GIF. |
| IMG-010 | P2 | Image cropping | DONE-U | Picture editor: Crop. |
| IMG-011 | P2 | Image annotation | PARTIAL | Freehand pen and highlighter only; no arrows, shapes or text labels. |
| IMG-012 | P2 | Offline OCR | DONE-U | Optional, OFF by default (spec: modules are off until enabled). Uses the OCR engine built into Windows through the Windows PowerShell that ships with Windows; nothing is downloaded. Not run (needs Windows). |
| IMG-013 | P2 | Color picker | NOT DONE | No colour picker for images (text colours like #0067C0 are recognised). |

## 7. Security and Privacy

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| SEC-001 | P0 | Fully offline operation | PARTIAL | No clipboard data ever leaves the PC. BUT, by your instruction (updates like Feather PDF), the program reads one small version file from GitHub at start. It can be switched off in Settings; with it off the program makes no network connections. |
| SEC-002 | P0 | Local-only storage | DONE-T | W: everything under %LOCALAPPDATA%\FeatherClipboardManager. |
| SEC-003 | P0 | Password-manager exclusion | DONE-T | W: default ignore list (KeePass, 1Password, Bitwarden...) plus the Windows "exclude from history" format flags. |
| SEC-004 | P0 | Pause clipboard monitoring | DONE-U | Pause recording (click the status, tray or More). Resuming does not recapture the unchanged clipboard. |
| SEC-005 | P0 | Private mode | DONE-U | Pause is the private mode (same behaviour); no separate switch. |
| SEC-006 | P0 | Secure local data permissions | DONE-U | A protected ACL (owner + SYSTEM only) is set on the data folder at start. Wine ignores ACLs, so untested. |
| SEC-007 | P0 | No telemetry | DONE-T | R: no telemetry code. The only network code is the version check. |
| SEC-008 | P1 | Application exclusion list | DONE-T | W: editable list of program names. |
| SEC-009 | P1 | Automatic history expiration | DONE-U | Delete unpinned items after N days (Settings). |
| SEC-010 | P1 | Sensitive-content detection | DONE-T | U: card numbers (Luhn), private keys, API tokens, JWTs, password-like strings, key=value secrets. Heuristic: it cannot find every secret. |
| SEC-011 | P1 | Automatic sensitive-entry removal | DONE-T | W: a sensitive item deletes itself after the set time (default 60 s) unless pinned. |
| SEC-012 | P1 | Local database encryption | NOT DONE | The history files are NOT encrypted at rest (only backups are encrypted). Protection is the per-user ACL; use BitLocker for disk encryption. |
| SEC-013 | P1 | Lockable clipboard history | NOT DONE | No lock with Windows authentication. |
| SEC-014 | P1 | Privacy notifications | DONE-U | A status-bar message tells you when a sensitive copy was skipped or will self-delete (no content shown). |
| SEC-015 | P2 | Protected collections | NOT DONE | Not implemented. |
| SEC-016 | P2 | Configurable redaction | DONE-U | Settings > Privacy > mask emails, phone numbers and tokens in the list and preview (U tests the masking). |
| SEC-017 | P2 | Local security audit | NOT DONE | No security audit view. |

## 8. Performance and Resource Optimization

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| PERF-001 | P0 | Event-driven monitoring | DONE-U | Listener, no polling. The only timers: a 5-second cleanup tick and short one-shot timers. |
| PERF-002 | P0 | Low idle CPU usage | DONE-U | Not measured on Windows. |
| PERF-003 | P0 | Low idle memory usage | PARTIAL | B (Wine): 100,000 items in memory = 178 MB working set, above the 60 MB goal at that history size. Idle memory with a small history was not measured on Windows. |
| PERF-004 | P0 | Fast popup activation | PARTIAL | Not measured on Windows; the window is created at start and only shown/hidden. |
| PERF-005 | P0 | Asynchronous database operations | PARTIAL | Capture/search run on the UI thread (recording a copy is one 0.25 ms append; search 25 ms at 100,000). OCR and downloads run on worker threads. Not fully asynchronous. |
| PERF-006 | P0 | Lazy loading | PARTIAL | Payload files are read only when needed; the first 1000 characters of every item are kept in memory. |
| PERF-007 | P0 | Database indexing | PARTIAL | No database; matching scans memory (B). See SEC/REL notes: SQLite/FTS5 from the spec was not used. |
| PERF-008 | P1 | Thumbnail caching | NOT DONE | No thumbnail cache. |
| PERF-009 | P1 | Configurable cache size | NOT DONE | No cache setting. |
| PERF-010 | P1 | Automatic database maintenance | DONE-U | Journal compaction at start and exit; cleanup every 5 s. |
| PERF-011 | P1 | Storage usage dashboard | PARTIAL | Statistics shows the total space used and counts, not a split into database/image/cache. |
| PERF-012 | P1 | Large-content safeguards | DONE-U | Settings > What to keep: largest single copy (default 20 MB). |
| PERF-013 | P2 | Performance diagnostics | PARTIAL | --bench command line mode only; nothing in the UI. |
| PERF-014 | P2 | Adaptive cache management | NOT DONE | Not implemented. |

## 9. Reliability, Recovery, and Data Integrity

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| REL-001 | P0 | Crash-resistant storage | DONE-T | W: journal append, temp-file-and-rename writes; torn last line test. |
| REL-002 | P0 | Graceful error handling | DONE-U | Plain-language messages for full disk, busy clipboard, missing files. |
| REL-003 | P0 | Safe shutdown | DONE-U | Compaction and settings saved on exit and on Windows shutdown. |
| REL-004 | P0 | Startup recovery | DONE-T | W: leftovers of interrupted writes removed at start; half-written journal line ignored. |
| REL-005 | P0 | Clipboard retry mechanism | DONE-U | Up to 25 attempts, 12 ms apart. There is no watchdog for a source that blocks inside delayed rendering. |
| REL-006 | P1 | Local backup | DONE-T | W: single .cmbackup file, optional password (AES-256-CBC, PBKDF2-SHA256 200,000 rounds, HMAC-SHA256). |
| REL-007 | P1 | Local restore | DONE-T | W: restore, wrong password refused, damaged file refused. |
| REL-008 | P1 | Export history | PARTIAL | Whole-history export only (the backup); no export of selected items to text/CSV. |
| REL-009 | P1 | Import history | DONE-T | W: import skips what you already have; path names are validated. |
| REL-010 | P1 | Database integrity checks | DONE-T | W: More > Tools > Check the history integrity. |
| REL-011 | P1 | Corruption recovery | PARTIAL | Records without a file are removed on request; a damaged index only loses the bad lines. |
| REL-012 | P2 | Versioned local backups | NOT DONE | Single backup file per export; no generations. |
| REL-013 | P2 | Diagnostic log viewer | DONE-U | More > Tools > Diagnostic log (content-free, bounded 256 KB). |
| REL-014 | P2 | Safe migration rollback | NOT DONE | No schema migrations exist yet (nothing to roll back). |

## 10. Windows Integration

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| WIN-001 | P0 | Windows 10/11 support | DONE-U | Built for Windows 10 1607+ and 11 (manifest, APIs). Run under Wine only. |
| WIN-002 | P0 | Native clipboard API integration | DONE-T | W: Win32 clipboard API, bounded retries. |
| WIN-003 | P0 | System tray menu | DONE-U | Tray menu incl. Copy a recent item. |
| WIN-004 | P0 | Startup with Windows | DONE-U | Optional, reversible (HKCU Run key, /tray). |
| WIN-005 | P0 | Native copy/paste compatibility | DONE-U | Ctrl+C / Ctrl+V untouched; global shortcuts are extra. |
| WIN-006 | P1 | Windows 11 visual integration | PARTIAL | Dark title bar and system theme/accent behaviour; no Mica/acrylic. |
| WIN-007 | P1 | High-DPI support | DONE-U | Per-monitor DPI v2 manifest, scaled layout. |
| WIN-008 | P1 | Multi-monitor support | DONE-U | Clamped to the monitor of the caret/cursor. |
| WIN-009 | P1 | Explorer integration | PARTIAL | Show in folder, copy file names as text. No Explorer shell extension. |
| WIN-010 | P1 | Windows notification integration | DONE-U | Tray balloons (optional, never contain copied text unless you turn on "notification when something is copied"). |
| WIN-011 | P1 | Portable mode | EXCLUDED | EXCLUDED by your decision: the app is NOT portable; all data lives in %LOCALAPPDATA%. |
| WIN-012 | P1 | Standard installer/uninstaller | DONE-T | Installer built (NSIS) and run under Wine: silent install, uninstall keeps history, /DELETEDATA removes it, shortcuts and Add/Remove entry created. |
| WIN-013 | P2 | Windows context-menu extensions | NOT DONE | No shell extensions. |
| WIN-014 | P2 | Virtual desktop awareness | NOT DONE | Not implemented. |
| WIN-015 | P2 | Command-line interface | PARTIAL | Only --tray, --exit, --selftest, --bench, --seed-demo; no commands that read or paste history. |

## 11. Text, Code, and Developer Tools

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| DEV-001 | P1 | Code-friendly previews | PARTIAL | Indentation and line breaks are preserved; the font is proportional (not monospace). |
| DEV-002 | P1 | Syntax highlighting | NOT DONE | No syntax highlighting. |
| DEV-003 | P1 | JSON formatting | DONE-T | U: format and validate (invalid JSON is left unchanged). |
| DEV-004 | P1 | XML formatting | DONE-T | U: format; ill-formed XML left unchanged; entities never resolved. |
| DEV-005 | P1 | URL recognition | DONE-T | U: links recognised and shown as LINK; opened only when you choose Open the link. |
| DEV-006 | P1 | Markdown preview | NOT DONE | No Markdown preview. |
| DEV-007 | P1 | Line numbering | DONE-T | U: Number the lines (on paste). |
| DEV-008 | P2 | JSON minification | DONE-T | U. |
| DEV-009 | P2 | Text diff viewer | DONE-U | Core LineDiff unit-tested; select two items > Compare (line by line only). |
| DEV-010 | P2 | Regex tester | NOT DONE | Regex search exists (SEARCH-014) but there is no regex tester dialog. |
| DEV-011 | P2 | Encoding utilities | DONE-T | U: Base64 and URL encoding of UTF-8. |
| DEV-012 | P2 | Hash generation | PARTIAL | SHA-256 only (U). |
| DEV-013 | P2 | SQL formatting | DONE-T | U: basic keyword line-breaking formatter. |
| DEV-014 | P2 | Path conversion | DONE-T | U. |
| DEV-015 | P2 | Unicode inspector | DONE-U | Core InspectUnicode unit-tested; context menu command not driven. |

## 12. Automation and Intelligent Offline Processing

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| AUTO-001 | P1 | Automatic text cleanup | DONE-U | On request only: Paste as > trim / remove blank lines / one line, and pipelines. |
| AUTO-002 | P1 | Content-aware actions | DONE-U | Open the link, Write an email, Open the path, Show as table, Edit picture. |
| AUTO-003 | P1 | Rule-based classification | DONE-T | U: links, emails, phones, colours, paths, JSON. |
| AUTO-004 | P1 | Automatic tagging | NOT DONE | Not implemented. |
| AUTO-005 | P2 | Rule engine | NOT DONE | Not implemented. |
| AUTO-006 | P2 | Offline text summarization | NOT DONE | Not implemented. |
| AUTO-007 | P2 | Offline language detection | NOT DONE | Not implemented. |
| AUTO-008 | P2 | Offline translation | NOT DONE | Not implemented. |
| AUTO-009 | P2 | Local semantic search | NOT DONE | Not implemented. |
| AUTO-010 | P2 | User-defined scripts | NOT DONE | Not implemented (deliberately: arbitrary scripts are executable code). |

## 13. Customization and Personalization

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| CUST-001 | P0 | Theme selection | DONE-T | V: light, dark, same as Windows. |
| CUST-002 | P0 | Configurable hotkeys | PARTIAL | See KEY-001. |
| CUST-003 | P1 | Accent color selection | DONE-U | Five highlight colours. |
| CUST-004 | P1 | Adjustable spacing | NOT DONE | No spacing setting. |
| CUST-005 | P1 | Configurable startup behavior | DONE-U | Start with Windows, start in tray, close to tray. |
| CUST-006 | P1 | Configurable history retention | DONE-U | Days, items, MB. |
| CUST-007 | P1 | Configurable paste behavior | DONE-U | Enter pastes or only copies. |
| CUST-008 | P1 | Settings import/export | NOT DONE | No settings export/import (settings are in HKCU\Software\FeatherClipboardManager). |
| CUST-009 | P2 | Custom CSS or theme definitions | NOT DONE | No theme files. |
| CUST-010 | P2 | Customizable entry actions | NOT DONE | Actions per type are fixed. |

## 14. Accessibility and Inclusive Design

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| A11Y-001 | P0 | Full keyboard navigation | DONE-U | Everything reachable by keyboard; dialogs use Tab and Esc. Not verified with a keyboard-only run-through on Windows. |
| A11Y-002 | P0 | Visible keyboard focus | PARTIAL | Focus rectangles on owner-drawn buttons; list and edit boxes use the system focus. |
| A11Y-003 | P0 | Screen-reader support | PARTIAL | Native list, edit and button controls expose names; the custom preview header and status bar are not exposed to screen readers. Not tested with Narrator. |
| A11Y-004 | P0 | Sufficient contrast | PARTIAL | Colours chosen for contrast but ratios were not measured. |
| A11Y-005 | P1 | High-contrast theme | DONE-U | Uses the Windows high-contrast colours when that theme is on. Not tested. |
| A11Y-006 | P1 | UI scaling | DONE-U | Text size setting and per-monitor DPI. |
| A11Y-007 | P1 | Reduced-motion support | DONE-U | The app has no animations. |
| A11Y-008 | P1 | Accessible status messages | PARTIAL | Status messages are shown in the status bar but not announced by a screen reader. |
| A11Y-009 | P2 | Custom accessibility profiles | NOT DONE | Not implemented. |

## 15. Maintenance, Diagnostics, and Distribution

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| MAINT-001 | P0 | Clean installation | DONE-U | Per-user, no administrator, no service, statically linked (no runtime to install). |
| MAINT-002 | P0 | Clean uninstallation | DONE-T | Run under Wine: asks (or /DELETEDATA) about history; keeps it by default. |
| MAINT-003 | P0 | Application version display | DONE-U | About dialog and file properties show 2.1.0. |
| MAINT-004 | P1 | Offline installer | DONE-T | Single self-contained installer (about 0.7 MB). |
| MAINT-005 | P1 | Portable executable distribution | EXCLUDED | EXCLUDED by your decision (not portable). |
| MAINT-006 | P1 | Local diagnostic logging | DONE-T | W: content-free local log. |
| MAINT-007 | P1 | Configurable logging | DONE-U | Settings > Privacy: switch the log off. |
| MAINT-008 | P1 | Offline update installation | PARTIAL | The installer can be run by hand offline; the in-app update downloads it (SHA-256 checked). Not code-signed. |
| MAINT-009 | P2 | Database repair utility | PARTIAL | Integrity check + repair (REL-010/011). |
| MAINT-010 | P2 | Self-test mode | DONE-T | --selftest result.txt (47 checks) and tests/run.sh (107 checks). |

## 16. Specialized Advanced Capabilities

| ID | Pri | Feature | Status | Notes / evidence |
|---|---|---|---|---|
| ADV-001 | P2 | Multiple clipboard slots | NOT DONE | Not implemented. |
| ADV-002 | P2 | Clipboard collections | PARTIAL | Groups act as named libraries; snippets are a reusable library. |
| ADV-003 | P2 | Drag-and-drop paste | NOT DONE | Not implemented. |
| ADV-004 | P2 | Table-aware clipboard handling | PARTIAL | Tables from Excel/Word paste as tables because HTML/RTF are kept; no structural handling. |
| ADV-005 | P2 | CSV/TSV preview | DONE-U | Core FormatTable unit-tested; "Show as a table" command not driven. |
| ADV-006 | P2 | Clipboard timeline | NOT DONE | Not implemented. |
| ADV-007 | P2 | Temporary clipboard sessions | NOT DONE | Not implemented. |
| ADV-008 | P2 | Local plugin architecture | NOT DONE | Not implemented (deliberately). |
| ADV-009 | P2 | Clipboard event debugger | PARTIAL | The format inspector shows the current clipboard state; no event log. |
| ADV-010 | P2 | Per-application paste profiles | NOT DONE | Not implemented. |
