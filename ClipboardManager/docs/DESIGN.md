# Design notes

## Why these choices
* **C++17 + Win32, statically linked**, no framework or third-party library: small (about 1.7 MB), nothing to install first, predictable resource use. It was cross-compiled with MinGW-w64.
* **Files + journal instead of SQLite.** The specification recommends SQLite with FTS5; SQLite was not used (no network was available to fetch a pinned copy, and the plain-files design keeps the history readable and recoverable by hand). The cost is stated in REQUIREMENTS.md: no real database indexes, no SQLite integrity tooling; matching scans memory (about 25 ms for one word at 100,000 items) and fuzzy/regex are slower.
* **Per-user, no service, no administrator.**

## Modules (`src/`)
| File | Role |
|---|---|
| `core.cpp` | Portable logic, unit-tested natively: text transforms, JSON/XML/SQL formatting, Base64/URL/SHA-256, diff, search-query parser, smart-type and sensitive-content detection, masking, UTF-8, `latest.json` reader, version comparison |
| `store.cpp` | History: files + append-only journal, duplicate bump, retention/cleanup, search matching, integrity check, private ACL |
| `clip.cpp` | Clipboard capture (text, HTML, RTF, PNG/DIB, file drop; privacy flags; bounded retries) and restore |
| `ui.cpp` | Main window, list, preview, tray, hotkeys, paste, menus, update flow |
| `dialogs.cpp` | Settings, text editor, password prompt, About, statistics, shortcut list, format inspector, update prompt, download progress |
| `imgedit.cpp` | Picture editor |
| `backup.cpp` | Backup file format and import/export |
| `ocr.cpp` | Optional OCR through Windows PowerShell + Windows.Media.Ocr |
| `updater.cpp` | WinHTTP version check and download |
| `selftest.cpp` | `--selftest`, `--bench`, `--seed-demo` |
| `lang.cpp` | English, Hindi, Spanish tables (missing strings fall back to English) |

## Data on disk (`%LOCALAPPDATA%\FeatherClipboardManager`)
* `Clipboard contents\<YYYY-MM-DD HH.MM.SS.mmm>.txt` text (UTF-8), `.html`, `.rtf` next to it, `.png` pictures, `<…> files.txt` file lists (one path per line), `<…> snippet.txt`, `.ocr.txt` recognised picture text.
* `Clipboard contents\index.txt` — the **journal**: a header line, then one line per change, tab-separated: `id kind pinned created deleted expires base source group title hash width height bytes flags dropeffect count lines smarttype head ~`. The last line for an id wins; `-<TAB>id` deletes; a line without the final `~` (torn write) is ignored. Fields escape `\t \n \r \\`. The journal is rewritten compactly (temp file, then rename) at start (when it has grown) and at exit.
* `diagnostics.log` (content-free, max 256 KB), `ocr.ps1` (only if OCR is used).
* Settings: `HKCU\Software\FeatherClipboardManager`.

## Backup format (`.cmbackup`)
`"CMBK"`, version 1, flags (bit 0 = encrypted), salt[16], iv[16], payload length (u64), payload, 32-byte check. Payload = `u32 count` then entries (`u32 name length, name UTF-8, u64 size, data`). Encrypted: AES-256-CBC (PKCS#7) with a 64-byte key from PBKDF2-HMAC-SHA256, 200,000 rounds (first 32 bytes encrypt, last 32 key an HMAC-SHA256 over header and ciphertext). Unencrypted: SHA-256 over header and payload. Import verifies the check **before** reading anything, accepts only plain file names (no paths), skips items you already have and never overwrites existing files.

## Clipboard correctness
Capture is event-driven (`AddClipboardFormatListener`), debounced 40 ms, retries `OpenClipboard` up to 25 × 12 ms. One item = one coherent kind: files (if `CF_HDROP`), else text (+ HTML/RTF from the same clipboard state), else a picture. Formats `ExcludeClipboardContentFromMonitorProcessing`, `CanIncludeInClipboardHistory = 0` and `Clipboard Viewer Ignore` stop capture before anything is stored. The program's own writes are ignored through the clipboard sequence number. Restored file lists always say "copy", never "cut". Known gaps: no watchdog for a source application that hangs inside delayed rendering; the clipboard sequence number is not re-checked between formats of one snapshot.

## Privacy and security
* No clipboard content is ever sent anywhere. The only network code is `updater.cpp` (a GET of `latest.json` and, on request, the installer). With "Look for new versions" off, no connection is made.
* The data folder has a protected ACL (owner and SYSTEM). History files are **not encrypted at rest**; backups can be.
* Sensitive-content detection is a heuristic (Luhn card numbers, PEM private keys, common token prefixes, JWTs, `password=` pairs, mixed-class strings).
* Previews are plain text: HTML is never rendered, links are never opened automatically, XML entities are never resolved, regex patterns and decoded images are size-bounded.
* OCR (optional, off by default) runs Windows PowerShell hidden with a script written to the data folder; nothing is downloaded.

## Deliberate omissions
Cloud sync, accounts, telemetry, online services, a plugin/script engine (arbitrary code), portable mode (your decision), SQLite/FTS5, at-rest encryption of the history, Windows Hello lock. See REQUIREMENTS.md for every row.
