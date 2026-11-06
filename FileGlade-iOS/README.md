# FileGlade for iPhone and iPad

The native iOS companion to FileGlade for Mac, with a touch interface inspired by the same file explorer.

## Version 1.1 — automatic dashboard

FileGlade opens on **All files**, combining its own files with every saved folder grant and their subfolders. The app restores access automatically; you do not need to select a location each time. Results are cached on disk, indexed in the background, and refreshed while the dashboard is open and when returning to it. Search and type filters work across all indexed sources. Tap a folder to open the Folders tab.

iOS requires a one-time Files picker grant for folders outside FileGlade. Use **Allow folder access** or the dashboard gear to grant the broadest folder a provider permits. Its descendants then join the dashboard automatically. iOS does not let any third-party file explorer silently enumerate all other apps’ private files or ungranted providers. Revoked permissions must be renewed.

## Productivity tools

Tap the **wand** icon for batch rename with preview and session undo, ZIP archives, recursive search, content-based duplicates, storage analysis, SHA-256 checksums, UTF-8 text editing, PDF combining, and image resizing. Use workspace tabs to keep up to eight folders open with their own histories. Touch and hold items to star or color-label them; the **Library** tab has starred files, recents, and activity.

Metadata is local to this device and does not change Finder tags or synchronize with Mac. Save text edits before leaving the editor. ZIP tools use vendored ZIPFoundation 0.9.20 under the MIT license included with the project and app.

## Browse and organize

- Switch between icon and list views, sort files, and search names in the current folder.
- Use the **All files** dashboard for combined browsing. Its gear opens one-time folder access settings for iCloud Drive and supported providers.
- Use **FileGlade files** for the app's own Documents folder. It also appears inside the system Files app under FileGlade.
- Import files using the More menu. Import copies the selected files into the current folder.
- Tap a folder to browse it; tap a file for Apple's Quick Look preview. Some file formats may not have a preview.
- Touch and hold an item for copy, cut, rename, details, share, or Recently Deleted actions.
- Tap **Select** for multiple items. Copy or cut, navigate to another folder, then tap **Paste**.
- Existing files are preserved on paste or restore: a duplicate receives a name such as `Notes copy 2.txt`.
- Use the Share action to send a file to another app or save it elsewhere in Files.

**Recently Deleted** is maintained by FileGlade, separate from the system Files app. Items can be restored to their original location. They remain inside FileGlade until restored; uninstalling FileGlade removes them along with its other local data. There is no permanent-delete command in this version.

Folder access is limited to locations you grant through the Files picker and the app's own storage. If a provider revokes a saved permission, choose the folder again. A provider may make some locations read-only.

The Mac and iPhone apps do not include an automatic synchronization service. To work with the same documents, choose the same iCloud Drive or other shared provider folder on both devices. The storage provider handles synchronization. FileGlade itself makes no network requests.

## Build and run

Requires iOS 17 or later and a current Xcode installation.

1. Open `FileGlade.xcodeproj` in Xcode.
2. Under **Signing & Capabilities**, choose your development team. The project is initially configured for the development team found on this Mac.
3. Connect and unlock your iPhone, trust the Mac if requested, and select it as the run destination.
4. Press **Run**. Enable Developer Mode if iOS requests it.

Xcode must be signed into the Apple Account associated with your development team. This is a development build; its provisioning follows Apple's account and certificate validity rules.

For a simulator build without signing:

```sh
xcodebuild -project FileGlade.xcodeproj -scheme FileGlade \
  -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath /tmp/FileGlade-iOS-simulator \
  CODE_SIGNING_ALLOWED=NO build
```

The project includes ZIPFoundation 0.9.20 as vendored MIT-licensed source, so rebuilding needs no package download. `FileOperations.swift` implements coordinated file operations and recoverable deletion; `BrowserModel.swift` manages locations and navigation; `FileGladeApp.swift` supplies the interface, previews, and sharing.

Run `./test-file-operations.sh` on the Mac to exercise the shared file operations in a disposable temporary folder. The checks cover copy collisions, rename, cut/move, invalid names, recursive-copy protection, recoverable deletion, and restoring without overwriting an existing file.

Apple references: [Directory access](https://developer.apple.com/documentation/uikit/providing-access-to-directories), [Run on a device](https://help.apple.com/xcode/mac/current/en.lproj/dev5a825a1ca.html), [Developer Mode](https://developer.apple.com/documentation/xcode/enabling-developer-mode-on-a-device/).

## Group by and About

Use **Group by** on All files or beside Sort inside a folder. Choose None, Name (letter ranges), Kind, Date modified, Size, or Location. Grouping creates section headings; sorting controls the order inside each section. Groups in All files stay together as more results are loaded. Folder grouping works in icon and detail/list views.

About displays **V2.0** and **Designed by: Akshaya Simha**. On Mac, use the FileGlade app menu; on iPhone, use About on All files or About FileGlade in the folder More menu.
