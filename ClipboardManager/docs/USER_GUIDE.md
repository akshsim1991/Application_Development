# Feather Clipboard Manager — user guide

## First steps
1. Install and start it. It sits in the notification area (tray) and records what you copy while it runs. On first start it asks whether to start with Windows (you can change this any time in Settings).
2. Copy things as usual. Press **Ctrl+Shift+V** in any program to open the history next to your cursor, pick an item, press **Enter** to paste it there. **Esc** hides the window; the program keeps recording.
3. Right-click the tray icon to pause recording, open the history folder, change settings or exit.

## The window
Search box · filter button (Everything, Text, Pictures, Files, Pinned, Snippets, Copied today, Recently deleted, kinds of text, groups) · Paste, Copy, Pin, Delete, Folder, More · the list (newest first; type labels such as LINK, EMAIL, JSON) · the preview of the selected item · a status bar (click the right side to pause or resume).

## Searching
Type words: every word must be found (in the text, the program name, the group). Also:

| Type | Finds |
|---|---|
| `"exact phrase"` | the words in that order |
| `~colour` | a word, tolerating one typo |
| `re:^inv\d+` | a regular expression (first 400 characters of each item) |
| `app:chrome` | copied from that program |
| `type:link` | text, picture, files, snippet, link, email, phone, colour, path, json |
| `tag:work` | items in that group |
| `date:today` `date:yesterday` `date:7d` `date:2026-11-13` | when it was copied |

## Pasting
* **Enter** pastes into the program you were using; **Shift+Enter** pastes as plain text; **Ctrl+C** only copies. (Settings can make Enter only copy.)
* Without opening the window: **Ctrl+Alt+1 … 9** paste the 1st … 9th newest item; **Ctrl+Alt+P** pastes the newest as plain text; **Ctrl+Alt+N** pastes the next item of your **paste queue**.
* Right-click an item: *Paste as* (upper/lower/title/sentence case, trim, remove extra blank lines or line breaks, sort/unique/reverse/number lines, format JSON/XML/SQL, Base64, URL-encode, path slashes, SHA-256), *Paste with find and replace*, *Paste with today's date and time* (`{date} {time} {datetime} {iso} {year} {weekday} {month}`), *Paste through* a saved pipeline, *Paste joined with* (when several items are selected), *Add to the paste queue*, *Compare the two items*, *Inspect the characters*, *Show as a table*.
* If something prevents pasting (an elevated program, a protected screen), the item is still copied: press Ctrl+V yourself. The program cannot confirm that another program received the paste.

## Pinning, groups, snippets, deleting
* **Pin** (Ctrl+P) keeps an item from automatic cleanup. **Group** files items under a name (find them with `tag:`).
* **Snippets** (Ctrl+N, or right-click a text > Save as snippet) are reusable texts that are never deleted automatically. **Edit** (Ctrl+E) changes a text or snippet.
* **Delete** (Del) moves items to *Recently deleted* for 7 days; **Ctrl+Z** brings the last deletion back. Deleting a file item never touches the files themselves.

## Pictures
Pictures are stored as PNG. The preview scales them (double-click: actual size). Right-click > *Edit the picture*: crop, draw, highlight, rotate, resize (75/50/25/200 %), undo, then save as a new item, save and copy, or save as PNG/JPEG/BMP. *Copy the text in the picture* uses OCR, which is **off by default** (Settings > What to keep); it uses the engine built into Windows and sends nothing anywhere.

## Privacy
* A copy that a program marks as private (password managers do) is never recorded. Programs in the ignore list (Settings > Privacy) are never recorded.
* Copies that look like a card number, API key, token or password are, by default, recorded but **deleted after 60 seconds** (or not recorded at all, or kept — your choice). This is a heuristic: it cannot recognise every secret.
* Pause recording whenever you like. Windows' own clipboard history and other programs may keep copies independently of this one.
* The history folder is private to your Windows account. It is **not encrypted** (use BitLocker); backups can be password-protected.

## Backup and restore
More > Export backup (optional password) writes one `.cmbackup` file; Import adds what you do not already have. Backups to network or cloud-sync folders are your choice and outside the program's control.

## Updates
When it starts, the program reads a small version file from GitHub (nothing about you is sent). If a newer version is out it asks **Update now** or **Later** (or **Do not remind me about this version**). Update now downloads the installer, checks its SHA-256, closes the program, installs in the same place and starts it again; your settings and history stay. Switch this off in Settings > General; *Update to new version…* checks on demand.

## Uninstalling
Windows Settings > Apps, or *Uninstall Feather Clipboard Manager* in the Start menu. It asks whether to delete your history and settings; the default is to keep them.

## Where things are
Program: `%LOCALAPPDATA%\Programs\Feather Clipboard Manager` · history and log: `%LOCALAPPDATA%\FeatherClipboardManager` · settings: registry `HKCU\Software\FeatherClipboardManager`.

## Command line
`FeatherClipboardManager.exe /tray` start hidden · `--exit` ask the running copy to quit · `--selftest result.txt` run the self-test.
