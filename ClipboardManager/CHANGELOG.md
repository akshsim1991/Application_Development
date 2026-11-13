# Changelog

## 2.1.1
A new one-screen installer like Feather PDF's: the install location with a Change... button and a single Install button (Update or Repair when it is already installed), then progress, then the program starts. This fixes the last page of the old installer, where the "start automatically when I sign in" sentence was cut off; the program asks about starting with Windows the first time it runs instead.

## 2.1.0
Rebuilt from scratch as **Feather Clipboard Manager** (the earlier "Clipboard Manager 1.0" had no source in this repository). History now lives in `%LOCALAPPDATA%\FeatherClipboardManager`, not next to the program.

New: PNG/HTML/RTF/file capture, journal-based history (crash safe), search syntax (phrase, fuzzy, regex, app, type, tag, date), snippets, groups, pin, Recently deleted + undo, paste queue, paste as / pipelines / find-replace / templates / joined, JSON/XML/SQL/Base64/URL/SHA-256 tools, diff, Unicode inspector, table preview, smart types (link, email, phone, colour, path, JSON), sensitive-content detection with automatic expiry, ignore list, masking, picture editor, optional OCR, encrypted backup, integrity check, diagnostic log, format inspector, themes (light/dark/high contrast), accent colours, text size, languages (English, Hindi, Spanish), installer, update prompt like Feather PDF, self-test and benchmark modes.
