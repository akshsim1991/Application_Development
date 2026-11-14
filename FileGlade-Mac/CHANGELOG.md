# Changelog

## 3.3.0
- Transfers are journaled: if FileGlade is interrupted mid-copy (crash, power cut), it offers to finish the unfinished items on the next launch and removes half-written copies first.
- Pause and Resume for running transfers.
- Optional copy verification (SHA-256) in Settings ▸ General ▸ File safety; a bad copy is removed and reported.
- iPhone: "Computer access (USB)" turns on a read-only, pairing-code-protected server so a Mac running FileGlade can browse the iPhone's FileGlade folders and Photos over the cable. The iPhone app now has a build target for it; tested over a local socket on the Mac, not yet with a real phone.

## 3.2.0
- Fixed: double-clicking an item on its name or icon now opens it (previously only empty space on the row worked).
- Sort menu now has explicit Ascending and Descending choices.
- The toolbar adapts to the window width instead of being cut off; the details pane and second pane hide when the window is narrow.
- Glass theme redesigned: a calm frosted look with readable text.
- Faster file selection and folder opening; resizable panes and columns; themes (Colourful, Dark, AMOLED, Glass); colourful icons; Settings window.
- iPhone and iPad in the sidebar (camera roll through macOS), plus the USB bridge client for files and photos from the FileGlade app.
- Every build now has its own build number; `bump-version.sh` raises the release number.
