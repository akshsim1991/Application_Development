# FileGlade 2.0 — Mac and iPhone

These features are included on both devices.

| Feature | What it does |
| --- | --- |
| Automatic All files dashboard | One combined, searchable view of accessible folders and their descendants, with cached background indexing and automatic refresh. Mac discovers home, cloud folders, and drives; iPhone automatically reuses saved access grants. |
| Workspace tabs | Keep up to eight folders open with independent back/forward histories. Copy or cut, switch tabs, and paste. |
| File filters | Show documents, images, video, audio, archives, folders, files of 100 MB or more, items modified in the last seven days, or starred items. |
| Batch rename | Preview find/replace, prefixes, suffixes, and padded numbering. Extensions are preserved and conflicts checked before changes. |
| Undo batch rename | Undo the most recent batch during the current app session, with conflict checks. |
| ZIP creation and extraction | Compress selections or extract into a new folder, preserving existing files and checking ZIP integrity. |
| Search subfolders | Filter names by extension, type, minimum size, and modification date across the current folder and its descendants. |
| Duplicate detection | Group exact-content matches using SHA-256. Review every copy and the space used by extra copies. Nothing is removed automatically. |
| Storage analysis | See counts, file byte totals, storage by extension, and the largest files. |
| Checksum verification | Calculate and copy SHA-256 fingerprints, or compare a file with an expected checksum. |
| Stars and color labels | Mark important files and folders. Color markers appear in icon view. |
| Recents and activity | Return to opened files and folders and review the latest 100 recorded actions and outcomes. |
| Text editor | Create or edit small UTF-8 files. Saves detect changes made outside the editor. |
| PDF combining | Reorder selected PDFs and images, then create a new PDF while keeping the originals. |
| Image resizing and conversion | Export smaller JPEG or PNG copies at common sharing sizes, with image orientation applied. |
| Copy path | Copy an item's filesystem path from its context menu. |

## Where to find them

**Mac:** Open a folder from **All files**, then click **Power tools** above the file area or press **⇧⌘P**. Press **⌘T** or click the plus beside the workspace tabs to open another tab. Right-click files to star them, assign labels, or open tools for that selection.

**iPhone:** Open a folder from **All files** to enter the **Folders** tab, then tap the **wand** icon above the file area. Use **Select** to choose several files first. Touch and hold an item for stars, labels, rename, sharing, or tools for that item. The plus beside the workspace tabs opens another tab. **Library** has a bottom tab.

For example, select PDFs, open **Power tools → Combine PDFs & images**, arrange their order, and save a new combined PDF. Or select a folder, open **ZIP archives**, and compress it before sharing.

## Practical details

- Stars, labels, recent items, and activity are local to each app. Labels do not change Finder's tags. Workspace tabs and batch-rename undo last for the current app session.
- Choose the same iCloud Drive or other provider folder on both devices to work on shared documents. The provider handles synchronization; FileGlade has no separate sync service.
- iPhone access remains limited to the app's own storage and folders granted through Files. Some providers expose read-only folders.
- ZIP extraction supports ZIPs only, up to 50,000 entries and 4 GB expanded per archive. ZIP creation supports up to 50,000 entries. Symbolic links and password-protected archives are not supported.
- Recursive scans stop at 100,000 entries, skip symbolic links and package contents, and report skipped items. Storage reports file byte sizes, not physical allocation for APFS clones or compressed files. Search shows the first 500 matching items; storage shows the largest 200 files.
- Duplicate checks skip undownloaded iCloud files and unreadable files. Scans can be cancelled.
- PDF combining supports up to 2,000 pages. Unlock encrypted PDFs in another app first. Image conversion exports the first frame of multi-frame images.
- The text editor supports UTF-8 files up to 2 MB. Use **Save** before leaving; edits are not autosaved into the original file.
- iPhone Recently Deleted is inside FileGlade. Restore important files before uninstalling.

## Validation

Automated tests passed for rename previews, collisions, swaps and rollback; ZIP round trips with Unicode names and empty folders; traversal and integrity rejection; exact-content duplicates; external text-edit conflicts; multi-page PDF combining; image dimensions; and scan cancellation. Original browsing, copy/cut/paste, history, and hidden-file checks also passed.

Both apps were compiled. The iPhone 2.0 build was signed, installed, and launched on the connected iPhone 13.

## Paid-explorer reference points

[ForkLift](https://binarynights.com/) advertises tabs, advanced search, tags, activity/log views, favorite paths, folder sync, and remote connections. [Documents by Readdle](https://documents.io/app) advertises archives, PDF tools, conversion, and cloud workflows. These informed the additions; exact paid/free tiers vary by product.

Remote-server connections, automatic folder synchronization, encrypted vaults, OCR scanning, and full PDF editing are further possibilities and are not included in this build.

ZIP support uses vendored [ZIPFoundation 0.9.20](https://github.com/weichsel/ZIPFoundation/tree/0.9.20) under its MIT license. The license is included in both app distributions.

## Group by and About

Use **Group by** on All files or beside Sort inside a folder. Choose None, Name (letter ranges), Kind, Date modified, Size, or Location. Grouping creates section headings; sorting controls the order inside each section. Groups in All files stay together as more results are loaded. Folder grouping works in icon and detail/list views.

About displays **V2.0** and **Designed by: Akshaya Simha**. On Mac, use the FileGlade app menu; on iPhone, use About on All files or About FileGlade in the folder More menu.
