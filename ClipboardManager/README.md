# Feather Clipboard Manager 2.1

A fast, private clipboard history for Windows 10 and 11. It keeps everything you copy — **text, pictures and files** — on your own PC, lets you search it and paste it again from any program. No account, no cloud, no telemetry.

Designed and developed by Akshaya Simha.

* **Install:** run `releases/FeatherClipboardManager-Setup-2.1.0.exe` (per user, no administrator permission needed).
* **Open it:** `Ctrl+Shift+V` from any program (changeable). Pick an item and press Enter to paste.
* **Your history** is kept in `%LOCALAPPDATA%\FeatherClipboardManager\Clipboard contents` as ordinary files. It is **not** portable by design and is never removed by an update or a normal uninstall.
* **User guide:** [docs/USER_GUIDE.md](docs/USER_GUIDE.md) · **What is done and what is not:** [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md) · **Design, data format, privacy:** [docs/DESIGN.md](docs/DESIGN.md) · **Tests and measurements:** [docs/TESTING.md](docs/TESTING.md)

## Highlights

* Capture of text (with HTML/RTF formatting), pictures (PNG) and copied files (references), duplicate detection, source program, unlimited history by default.
* Instant search with `"exact phrases"`, `~fuzzy`, `re:regex`, `app:`, `type:`, `tag:`, `date:`.
* Paste, paste as plain text, paste as upper/lower/title case, trim, remove blank lines/line breaks, find & replace, format JSON/XML/SQL, Base64/URL encode, SHA-256, paths with `/` or `\`, date/time templates, saved **pipelines**, multiple items joined, a **paste queue**, snippets.
* Privacy: password-manager and "private" clipboard flags respected, program ignore list, sensitive-content detection (cards, keys, tokens, passwords) that deletes itself after 60 s, pause, masking, content-free log, per-user folder permissions.
* Pictures: crop, draw, highlight, rotate, resize, save as PNG/JPEG/BMP, optional offline OCR (Windows' built-in engine).
* Backup/restore as one file with optional AES-256 password; integrity check and repair; Recently deleted (7 days) with undo.
* Light/dark/high-contrast, English / हिन्दी / Español, tray icon, start with Windows (optional).
* Updates, like Feather PDF: when it starts it reads one small version file and asks "Update now" or later (switch off in Settings).

## Build

Source is C++17 / Win32 (no frameworks, no third-party libraries). On Linux (Ubuntu/Debian):

```sh
sudo apt-get install g++-mingw-w64-x86-64-posix nsis python3 python3-pil
scripts/make_installer.sh        # builds build/FeatherClipboardManager.exe and releases/FeatherClipboardManager-Setup-<version>.exe
tests/run.sh                     # unit tests of the portable core (any g++)
```

On Windows you can build with any MinGW-w64 g++ using the same command line as `scripts/build.sh`. See [docs/TESTING.md](docs/TESTING.md) for the self-test (`FeatherClipboardManager.exe --selftest result.txt`).

## Publishing an update

`scripts/publish-release.sh <checkout of akshsim1991/FeatherClipboardManager-releases> "what is new"` copies the installer into the public releases repository and writes `latest.json` (version, notes, URL, SHA-256). Running copies read that file. **That public repository must exist first** (create it once on GitHub, empty, branch `main`).

## Status

This release was built and tested without a Windows PC (cross-compiler + Wine). Read [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md): it lists, row by row, what is implemented, what was actually tested and what is missing.
