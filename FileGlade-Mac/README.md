# FileGlade

A native macOS file explorer inspired by Windows 11, built with SwiftUI and AppKit.


## Version 2.0 — automatic dashboard

FileGlade opens on **All files**, combining your home folder, detected iCloud Drive and CloudStorage providers, and connected drives in one dashboard. No daily location selection is needed. The background metadata index shows cached results immediately, refreshes while the dashboard is open (every 30 seconds after a scan finishes), and refreshes when returning to the app. Search finds names and paths across all indexed folders; use the type filter to narrow results. Click a folder to open the existing file browser, then click **All files** to return.

The first scan can take time. File contents are not read or downloaded by indexing. macOS protects some folders and may request permission; unreadable folders are reported under the information button. Hidden files and package contents are excluded by default; the information panel can include hidden files. Symbolic links are skipped.

## Productivity tools

Click **Power tools** above the file area or press **⇧⌘P** for batch rename with preview and session undo, ZIP creation/extraction, recursive search, duplicates, storage analysis, checksums, a UTF-8 text editor, PDF combining, and image resizing. **⌘T** opens a workspace tab; up to eight tabs keep independent navigation histories. Right-click items to star them or assign local color labels. The library inside Power tools contains starred files, recents, and activity.

Metadata is local to FileGlade on this device. Labels do not modify Finder's tags or synchronize with iPhone. Save text edits before leaving the editor. Archives and converted documents preserve existing output files by choosing a new name.

## What's new in 2.0 (Mac)

- **Dashboard:** recent files, Downloads, large files and storage-by-type cards; combined date/size/location filters; saved searches; **Smart folders** in the sidebar (saved searches that never move files).
- **Saved preferences:** sort, grouping, view, details pane, filter and open tabs are remembered.
- **Transfers:** background copy/move with progress, speed, time left, Cancel and Retry; Keep Both / Replace / Skip conflict choices; **Undo** (⌥⌘Z) for copies, moves, renames and Trash.
- **Delete Permanently** (⌥⌘⌫) skips the Trash and cannot be undone; it always asks first.
- **Previews:** image, PDF, audio/video and text previews in the details pane; thumbnails in icon view.
- **Drag and drop** between folders, tabs, the sidebar and other apps (⌥ forces copy).
- **Permissions:** FileGlade is signed with a stable identity so macOS remembers folder access. Turn on Full Disk Access once (FileGlade menu ▸ Full Disk Access Settings…) and reopen the app to stop prompts entirely; until then Desktop, Documents, Downloads, iCloud and drives are left out of All files.

## What's new in 3.0 — power basics (Mac)

- **Dual-pane browsing** (⌘\\): two folders side by side. F-key style copy/move to the other pane (⌥⌘C / ⌥⌘M), Tab switches panes, drag and drop works between them.
- **Selection basket** (⌘B): collect files from many folders, then copy, move, compress or share them together.
- **Compare and sync folders**: shows what is missing or newer on each side and copies it, without deleting anything; replaced files go to the Trash and can be undone.
- **Advanced multi-rename**: regular expressions, letter case, spaces, insert dates, numbering and saved presets.
- **Archive browsing** (⌥⌘B or right-click): look inside ZIP, TAR, 7z, RAR, ISO and more, and extract just what you need.
- **Search inside documents** (⇧⌘F): finds words inside PDFs, Word files, text and code using Spotlight's index, with excerpts.
- **Keyboard shortcuts** (FileGlade menu): choose the **Mac** or **Windows** layout (Enter opens, F2 renames, Delete trashes, F5 refreshes) and change any shortcut.
- **Automatic updates**: FileGlade checks for new versions, asks whether to update now or later, verifies the download and restarts itself. Use FileGlade ▸ Check for Updates… at any time.
- **Much faster browsing**: folders load off the main thread, sorting and filtering are cached, icons are reused and the background index no longer rescans constantly.


Double-click **FileGlade.app**. You can drag it to your Applications folder for convenient access. The included build runs on Apple silicon Macs with macOS 13 or later.

## What you can do

- Browse local folders, connected drives, and folders exposed by your cloud storage apps.
- Use icon or detail views, folders-first sorting, breadcrumbs, back/forward history, and an editable location bar.
- Inside a folder, search filenames in that folder. The All files dashboard searches across every indexed source.
- Open files in their default apps, or reveal them in Finder.
- Create folders, rename files, copy, cut, paste, and move selected items to Trash after confirmation.
- Command-click for multiple selections; use Shift-click for a range in icon view.
- Pin the current folder from the details pane or the More menu.
- See file size, type, location, modified date, and available disk space.
- Drag items out to other apps. Paste files copied in Finder.
- Show hidden files and automatically refresh when the current folder changes.

Existing files are preserved when pasting: duplicates receive a name such as `Notes copy 2.txt`. Cut files are moved when you paste. Items sent to Trash can be restored using Finder. There is no general undo for file operations; Power tools provides session undo for its most recent batch rename.

FileGlade follows your system's light/dark appearance. macOS may ask for access when you first browse protected folders. It runs locally and makes no network requests. It does not change your default file manager.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| ⌘O | Choose a folder |
| ⇧⌘O | Open the selected item |
| ⇧⌘N | New folder |
| Return | Rename selected item |
| ⌘C / ⌘X / ⌘V | Copy / cut / paste |
| ⌘A | Select all visible items |
| ⌘Delete | Move selection to Trash |
| ⌘F | Search this folder |
| ⌘L | Edit folder path |
| ⇧⌘G | Go to folder |
| ⌘[ / ⌘] | Back / forward |
| ⌘↑ | Parent folder |
| ⇧⌘H | Home |
| ⌘1 / ⌘2 | Icons / details |
| ⌥⌘I | Toggle details pane |
| ⇧⌘. | Show/hide hidden files |
| ⌘R | Refresh |
| Escape | Clear selection and search when not editing text |

Arrow keys navigate the icon view and the native detail table.

## Source and rebuild

`Source/main.swift` contains the app. `Source/AppIcon.icns` is the included icon; `Source/make-icon.swift` contains its original drawing code.

Install Apple's Command Line Tools, then run from this folder:

```sh
./build.sh
open FileGlade.app
```

The build script compiles for the current Mac's architecture and applies a local ad hoc signature. This personal build is not Apple-notarized for public distribution.

Run the file-operation checks with:

```sh
FileGlade.app/Contents/MacOS/FileGlade --self-test
```

Tests create a disposable temporary folder and exercise listing, folders-first sorting, filename filtering, duplicate-safe copies, cut/paste, navigation history, and hidden files. They temporarily use the macOS clipboard and restore its contents when finished.

## Power-tool validation and library

Run `./test-power-tools.sh` for the new operations' temporary-file tests. ZIPFoundation 0.9.20 is vendored under `Source/ZIPFoundation` with its MIT license; builds need no package download. `PowerCore.swift` and `PowerTools.swift` are shared with the iPhone project.

Run `./test-dashboard.sh` for temporary-file checks of aggregation, overlapping sources, search, filters, persistence, refresh, and cancellation. The command needs access to the macOS file-type registry.

## Group by and About

Use **Group by** on All files or beside Sort inside a folder. Choose None, Name (letter ranges), Kind, Date modified, Size, or Location. Grouping creates section headings; sorting controls the order inside each section. Groups in All files stay together as more results are loaded. Folder grouping works in icon and detail/list views.

About displays **V2.0** and **Designed by: Akshaya Simha**. On Mac, use the FileGlade app menu; on iPhone, use About on All files or About FileGlade in the folder More menu.
