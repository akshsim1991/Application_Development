# Changelog

## 2.2.0
* **Shortcut picker:** the three global shortcuts (open the history, paste newest as plain text, paste next of the queue) are set by pressing the keys you want in Settings > Shortcuts. Shortcuts that would break typing or Windows (a bare key, Ctrl+C, Alt+F4, Win+L ...) are refused with the reason; two actions cannot share one shortcut; a shortcut another program already owns is reported. Earlier choices are carried over.
* **Free-disk-space guard:** recording stops, with a message and a log entry, when less than the chosen amount (default 200 MB) is free on the history's disk (Settings > What to keep).
* **Delete everything:** More > Delete everything... removes every item for good (pinned items and snippets too) with all their files, text read from pictures and stray files, after a clear warning that it is not secure erasure.
* **Search highlighting in the list:** the words you typed are highlighted in each row; long texts show the part around the first match.
* **Code signing:** the build can sign the program and installer (`scripts/sign.sh`); see docs/SIGNING.md. The published installer is not signed until a certificate is available.

## 2.1.1
A new one-screen installer like Feather PDF's: the install location with a Change... button and a single Install button (Update or Repair when it is already installed), then progress, then the program starts. This fixes the last page of the old installer, where the "start automatically when I sign in" sentence was cut off; the program asks about starting with Windows the first time it runs instead.

## 2.1.0
Rebuilt from scratch as **Feather Clipboard Manager** (the earlier "Clipboard Manager 1.0" had no source in this repository). History now lives in `%LOCALAPPDATA%\FeatherClipboardManager`, not next to the program.

New: PNG/HTML/RTF/file capture, journal-based history (crash safe), search syntax (phrase, fuzzy, regex, app, type, tag, date), snippets, groups, pin, Recently deleted + undo, paste queue, paste as / pipelines / find-replace / templates / joined, JSON/XML/SQL/Base64/URL/SHA-256 tools, diff, Unicode inspector, table preview, smart types (link, email, phone, colour, path, JSON), sensitive-content detection with automatic expiry, ignore list, masking, picture editor, optional OCR, encrypted backup, integrity check, diagnostic log, format inspector, themes (light/dark/high contrast), accent colours, text size, languages (English, Hindi, Spanish), installer, update prompt like Feather PDF, self-test and benchmark modes.
