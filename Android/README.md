# Feather PDF for Android

The first Android version (2.21.0 "Puma") has the essential reading and editing. It uses the same PDF engine as
the Windows and Mac apps (`shared/`): a page looks the same, and a paragraph is edited the same way, on all three.

## What it does

| Area | On Android |
|---|---|
| Opening | Open a PDF (the system's file picker), "Open with" and "Share to" from other apps, password-protected files, recent files (with the page last read; long-press to remove one) |
| Reading | Continuous scrolling, pinch to zoom (up to 8x) and double tap, sharp pages when zoomed in, go to page, bookmarks, find with every match highlighted (previous / next), page colours: normal, night, dimmed, blackboard (inverted, or with pictures in their own colours) |
| Text | Long-press and drag to select text: copy, highlight, underline, strike out |
| Editing text | Edit text: every paragraph is outlined; tap one to change its words, size, bold or italic (the engine's paragraph editing: the text wraps in its column and moves what is below it) |
| Adding | Add text (a new line of text), notes, highlights (sweep a finger over the text), drawing with a pen (colour and width: hold Draw; two fingers still move and zoom) |
| Removing | Erase: tap a highlight, drawing or note to remove it; a note's comment can be read, changed or removed by tapping it |
| Pages | Turn the page on screen, delete it |
| History | Undo and redo every change |
| Settings | As on Windows: Appearance (theme, where the tools are), PDF Viewing (page colours, last page read, screen on), Editing (highlight and drawing colours, line width), Performance (pages kept ready), Advanced (default app, recent files, versions, About, reset) |
| Saving | Save (back to the file it came from, when that place allows it), save a copy (anywhere the system offers), share; leaving with unsaved changes asks first |

Not yet on Android (to come): forms, signatures, tables, pictures (insert, move), page thumbnails, combining and
splitting, redaction, OCR, printing, and the many other tools of the desktop apps.

## How it is built

- `app/src/main/cpp`: `bridge.cpp` (JNI) and the shared engine, built for arm64-v8a, armeabi-v7a and x86_64. PDFium is
  the same release as on the other systems (chromium/8066, `scripts/fetch-pdfium.sh android`).
- `app/src/main/java/com/featherpdf/engine/Engine.java`: the engine's calls, in plain Java.
- `app/src/main/java/com/featherpdf/android`: the app, in Kotlin, with the Android framework only (no AndroidX):
  `DocumentView` (pages, zoom, the tools' touches), `ViewerActivity` (one document), `MainActivity` (start screen),
  `SelfTest` (the build check on an emulator).
- The cryptography (protected files) and pictures come from Android's own classes through `Engine.SystemHooks`.

Build (needs the Android SDK and NDK 27): `bash scripts/fetch-pdfium.sh android`, then in `Android/`:
`./gradlew assembleRelease`, which makes one APK for each kind of phone (`app-arm64-v8a-release.apk` for nearly all
phones, `app-armeabi-v7a-release.apk` for older 32-bit ones, `app-x86_64-release.apk` for emulators and Chromebooks),
each about a third of one APK for all. GitHub builds it on every push (`.github/workflows/android.yml`) and puts the
first two in `releases/` as `FeatherPDF-<version>-android.apk` and `FeatherPDF-<version>-android-32bit.apk`.

## Checks

- `tests/android/bridge_check.sh`: the bridge built for Linux and driven from Java with the system services given as on
  Android (opening, password files, drawing and page colours, selection, editing a paragraph, annotations, finding,
  pages, undo and redo, saving and reopening, saving a protected file): 49 checks.
- `tests/android/compile`: the app's code compiled against the Android framework's classes (no SDK needed).
- The workflow builds the APK, checks it holds every ABI, is signed with the release key and is aligned for 16 KB pages,
  then runs it on an Android 14 emulator: `SelfTest` goes through the screen (edit mode, a paragraph changed, a
  highlight, a note, a stroke drawn with touches, finding, night colours, undo, redo, saving) and takes pictures; the
  saved file is then read by other tools (pikepdf, pdfminer).

## The signing key

Android installs an update only when it is signed with the same key as the installed app. The key
(`app/featherpdf-release.jks`, password in `app/build.gradle.kts`) is kept with the code so that every build, on any
machine, can sign updates. Anyone with this repository can sign an APK as Feather PDF; to keep the key private, put it
in GitHub secrets and set `FEATHER_KEYSTORE` and `FEATHER_KEYSTORE_PASSWORD` in the workflow (the same key, so installed
copies still update). Its SHA-256 fingerprint is
`5E:CE:1A:1B:B3:3B:10:42:47:F9:BF:FD:50:69:A2:94:6B:8F:7D:E3:7C:CB:51:17:6E:75:1F:0B:5E:BB:EE:12`.

## Known limits

- An unsaved change is lost if Android closes the app in the background (it keeps a document while you switch apps,
  but may close it when the phone is short of memory). Save before leaving for long.
- Edited text uses the PDF's own font or a standard one (Helvetica, Times, Courier); fonts installed on the phone are
  not used yet. Letters outside Western languages cannot be typed into a paragraph yet (as on the other systems).
- The APK is installed by hand (no app store yet), and does not look for new versions itself.
